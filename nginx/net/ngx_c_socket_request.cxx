#include "ngx_c_conf.h"
#include "ngx_c_socket.h"
#include "ngx_c_comm.h"
#include "ngx_c_memory.h"
#include "ngx_c_lockmutex.h"
#include "ngx_c_threadpool.h"
#include "ngx_c_slogic.h"

extern CLogicSocket g_socket;       //socket全局对象
extern CThreadPool g_threadpool;   //线程池全局对象

//该函数是epoll对象上与客户端连接对应的套接字描述符上面的读事件的处理函数。
void CSocket::ngx_read_request_handler(lpngx_connection_t c)
{
    //收包。recvproc()函数在下面定义，调用了系统的recv()函数。注意我们用的第二和第三个参数，我们用的始终是这
    //两个参数，因此必须保证c->precvbuf指向正确的收包位置。
    //printf("客户端来数据了\n");
    ssize_t reco = recvproc(c,c->precvbuf,c->irecvlen);
    if( reco <= 0 )
    {
        return ;
    }
 
    if( c->curStat == _PKG_HD_INIT ) //连接建立起来时肯定是这个状态
    {
        //收到的字节数刚好等于包头的长度
        if( reco == m_iLenPkgHeader )   //正好收到完整包头，这里拆解包头
        {
            ngx_wait_request_handler_proc_p1(c);    //调用专门针对包头处理完整的函数去处理(包头处理完整之后需要进行的操作)。
        }
        else 
        {
            c->curStat = _PKG_HD_RECVING;              
            c->precvbuf = c->precvbuf + reco;      
            c->irecvlen = c->irecvlen - reco;         
        }
    }
    else if( c->curStat == _PKG_HD_RECVING )    //接收包头中。【包头不完整，继续接收中】
    {
        if( c->irecvlen == reco )   //包头收完了
        {
            ngx_wait_request_handler_proc_p1(c);
        }
        else 
        {
            //包头还没收完整，继续收包头
            c->precvbuf = c->precvbuf + reco;
            c->irecvlen = c->irecvlen - reco;
        }
    }
    //接收包体的初始状态
    else if( c->curStat == _PKG_BD_INIT )
    {
        if( reco == c->irecvlen )
        {
            //包体也接收完整了
            ngx_wait_request_handler_proc_plast(c);
        }
        else 
        {
            c->curStat = _PKG_BD_RECVING;
            c->precvbuf = c->precvbuf + reco;
            c->irecvlen = c->irecvlen - reco;
        }
    }
    //包体没收完，接收包体中
    else if( c->curStat == _PKG_BD_RECVING )
    {
        if( c->irecvlen == reco )
        {
            ngx_wait_request_handler_proc_plast(c);
        }
        else 
        {
            c->precvbuf = c->precvbuf + reco;
            c->irecvlen = c->irecvlen - reco;
        }
    }

    return ;
}


//recv()的安全版本封装
ssize_t CSocket::recvproc(lpngx_connection_t c,char* buff,ssize_t buflen)
{
    ssize_t n;
    //recv参数：套接字描述符、用于接收数据的缓冲区、缓冲区长度、一组标志（通常可设为0）。
    //int n = recv(c->fd,buf,2,0);
    n = recv(c->fd,buff,buflen,0);
    if( n == 0 )
    {
        if( close(c->fd) == -1 )
        {
            printf("CSocket::recvproc()中close(%d)失败!\n",c->fd);
        }
        inRecyConnectQueue(c);  //延迟回收连接池
        //客户端关闭连接才会n==0（应该是正常完成了4次挥手），这边就直接收回连接，关闭socket即可。
        printf("连接被客户端正常关闭!\n");
        //ngx_close_connection(c); 
        return -1;
    }
 
    //客户端没断，走这里
    if( n < 0 )     //有错误发生
    {
        //EAGAIN和EWOULDBLOCK(惠普)应该是一样的值，表示没收到数据，一般来说，在ET模式下会出现这个错误。
        if( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            printf("CSocket::recvproc中errno == EAGAIN || errno == EWOULDBLOCK成立了！出乎意料！\n");
            return -1;
        }

        if ( errno == EINTR )   //官方nginx中这个不算错误。
        {
            printf("CSocket::recvproc中errno == EINTR成立！出乎意料！\n");
            return -1;      //不当作错误处理,只是简单返回
        }

        //从这里往下的错误都认为是应该当作错误处理。意味着我们应该关闭客户端套接字，回收连接池中的连接。
        if( errno == ECONNRESET )
        {
            //客户端没有发送四次挥手的数据包来断开连接，而是发送rst复位包来断开连接。
            //一般来说，这种情况出现在客户端没有正常关闭socket连接，而是直接关闭了整个应用程序。

        }
        else 
        {
            printf("CSocket::recvproc中发生了错误！\n");
        }
        
        if( close(c->fd) == -1 )
        {
            printf("CSocket::recvproc()中close_2(%d)失败!\n",c->fd);
        }
        inRecyConnectQueue(c);
        
        printf("连接被客户端 非 正常关闭!\n");
        //ngx_close_connection(c);
        return -1;
        
    }//end of if( n < 0 ) 

    //程序走到这里，认为收到了有效数据
    return n;       //返回收到的字节数

}

//包头收完整之后的处理函数。记为包处理阶段p1。
void CSocket::ngx_wait_request_handler_proc_p1(lpngx_connection_t c)
{
    CMemory *p_memory = CMemory::getInstance();
    LPCOMM_PKG_HEADER pPkgHeader;
    pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;        //dataHeadInfo保存每个连接的包头数据

    //printf("收到的包的报文总长度为:%d,消息类型为%hu\n",pPkgHeader->pkgLen,pPkgHeader->msgCode);

    //变量e_pkgLen保存整个数据包的大小，从接收到的包头中的pkgLen字段获得
    unsigned short e_pkgLen;
    e_pkgLen = ntohs(pPkgHeader->pkgLen);
    
    //整个包的长度小于包头的长度，恶意包或错误包
    if( e_pkgLen < m_iLenPkgHeader )
    {
        c->curStat = _PKG_HD_INIT;
        //重新收包头
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
        return ;
    }
    //认为客户端不会发来包长度大于_PKG_MAX_LENGTH-1000的包
    else if( e_pkgLen > ( _PKG_MAX_LENGTH - 1000 ) ) 
    {
        c->curStat = _PKG_HD_INIT;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
        return ;
    }
    //合法的包头
    else 
    {
        //new出一段内存来收包。内存大小为消息头大小+整包大小（包括包头）。
        char* pTmpBuffer = (char*)p_memory->AllocMemory(m_iLenMsgHeader+e_pkgLen,false);
        c->precvMemPointer = pTmpBuffer;

        LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
        //填写消息头内容
        //消息头中记录连接的指针
        ptmpMsgHeader->pConn = c;
        ptmpMsgHeader->iCurrsequence = c->iCurrsequence;
        //填写包头内容
        pTmpBuffer += m_iLenMsgHeader;
        memcpy(pTmpBuffer,pPkgHeader,m_iLenPkgHeader);
        if( e_pkgLen == m_iLenPkgHeader )
        {
            //这个数据包只有包头而没有包体，相当于包收完整了，直接扔进消息队列，让后续的业务逻辑处理
            ngx_wait_request_handler_proc_plast(c);
        }
        else 
        {
            //包头处理完了，设置相应的值，准备接收包体
            c->curStat = _PKG_BD_INIT;
            c->precvbuf = pTmpBuffer + m_iLenPkgHeader;
            c->irecvlen = e_pkgLen - m_iLenPkgHeader;
        }
    }
    return ;
}

//一个包收完整后的触发函数
void CSocket::ngx_wait_request_handler_proc_plast(lpngx_connection_t c)
{
    //消息头+包头+包体的内容的内存首地址就存在了c->precvMemPointer里面。
    g_threadpool.inMsgRecvQueueAndSignal(c->precvMemPointer);    //入消息队列并激发一个线程来处理消息

    //为收下一个包做准备
    c->precvMemPointer = NULL;
    c->curStat = _PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
    return ;
}

// void CSocket::inMsgRecvQueue(char* buf,int& irmqc)
// {
//     //printf("收到了一个完整的数据包！接下来在inMsgRecvQueue()中处理这个包！\n");
//     CLock lock(&m_recvMessageQueueMutex);
//     m_MsgRecvQueue.push_back(buf);
//     ++m_iRecvMsgQueueCount;
//     irmqc = m_iRecvMsgQueueCount;           //消息队列中当前的消息数量赋值给irmqc
 
// }

// //从消息队列m_MsgRecvQueue中取出一个消息，返回该消息。
// char* CSocket::outMsgRecvQueue()
// {
//     CLock lock(&m_recvMessageQueueMutex);
//     if( m_MsgRecvQueue.empty() )
//     {
//         return NULL;
//     }
//     char* sTmpMsgBuf = m_MsgRecvQueue.front();
//     m_MsgRecvQueue.pop_front();
//     --m_iRecvMsgQueueCount;
//     return sTmpMsgBuf;
// }

void CSocket::threadRecvProcFunc(char* pMsgBuf)
{
    printf("执行了父类的threadRecvProcFunc()\n");
    return ;
}

//发送数据专用函数，返回本次发送的字节数。返回值：
// > 0：成功发送了一些字节；
// == -1：errno == EAGAIN, 本方发送缓冲区满了
// == -2：errno != EAGAIN != EWOULDBLOCK != EINTR， 应该都是对端断开的错误
ssize_t CSocket::sendproc(lpngx_connection_t c, char* buff, ssize_t size)
{
    //这里参考了官方nginx函数ngx_unix_send()的写法。
    ssize_t n;
    
    while( 1 )
    {
        n = send(c->fd, buff, size, 0);
        if( n > 0 )
        {
            //成功发送了一些数据，至于发送了多少，这里不关心，也不需要再次send
            //（1）n == size，也就是想发送多少数据都发送成功了，这表示完全发送完毕了
            //（2）n < size，没发送完毕，那肯定是发送缓冲区满了，所以也不必重试发送，直接返回
            return n;   //返回本次发送的字节数
        }
        if( n == 0 )
        {
            //send()返回0？一般recv()返回0表示断开，send()返回0，这里就直接返回0吧，让调用者处理；
            //网上找资料：send==0表示超时，对方主动关闭了连接过程
            //我们写代码遵循一个原则，连接断开，我们并不在send()动作里面处理诸如关闭socket这种动作，集中到recv那里处理，
            //连接断开epoll会通知并且recvproc()里会处理，不用在这里处理。
            return 0;
        }
        if( errno == EAGAIN )
        {
            //内核发送缓冲区满
            return -1;
        }
        if( errno == EINTR )
        {
            //参考官方的写法，打印个日志，其他啥也没干，那就是等下次for循环重新send试一次了。
            printf("CSocket::sendproc()中send()失败\n");
        }
        else 
        {
            //errno 既不是EAGAIN 也不是 EINTR
            return -2;
        }
    }//end while(1)

}
 
//该函数是epoll对象上与客户端连接对应的套接字描述符上面的写事件的处理函数。
void CSocket::ngx_write_request_handler(lpngx_connection_t pConn)
{
    printf("调用了ngx_write_request_handler()\n");
    CMemory* p_memory = CMemory::getInstance();
    ssize_t sendsize = sendproc(pConn,pConn->psendBuf,pConn->isendLen);
    //没有全部发送完毕，数据只发送出去了一部分，等下次fd上还有写就绪，再次调用该函数发送。
    if( sendsize > 0 && sendsize != pConn->isendLen )
    {
        pConn->psendBuf = pConn->psendBuf + sendsize;
        pConn->isendLen = pConn->isendLen - sendsize;
        return ;
    }
    else if( sendsize == -1 )
    {
        printf("CSocket::ngx_write_request_handler()时if( sendsize == -1 )成立了，这很奇怪。\n");
        return ;
    }
    //数据成功发送完毕
    else if( sendsize > 0 && sendsize == pConn->isendLen )
    {
        //把写事件从fd上去掉
        if( ngx_epoll_ope_event(
                                pConn->fd,
                                EPOLL_CTL_MOD,
                                EPOLLOUT,
                                1,
                                pConn
                                ) == -1 )
        {
            printf("CSocket::ngx_write_request_handler()中ngx_epoll_oper_evgent()失败!\n");
        }
        printf("CSocket::ngx_write_request_handler()中数据发送完毕，并将epoll上相应fd的写事件去掉了\n");
    }

    //程序走到这里，要么数据发送完了，要么对方断开了连接(else if(sendsize == 0))
    if( sem_post(&m_semEventSendQueue) == -1 )
    {
        printf("CSocket::ngx_write_request_handler()中sem_post(&m_semEventSendQueue)失败!\n");
    }
    p_memory->FreeMemory(pConn->psendMemPointer);
    pConn->psendMemPointer = NULL;
    --pConn->iThrowsendCount;
    return ;
}

