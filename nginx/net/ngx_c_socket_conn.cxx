//和 网络 中 连接 有关的函数放在这里

#include "ngx_c_socket.h"
#include "ngx_c_conf.h"
#include "ngx_c_memory.h"
#include "ngx_c_lockmutex.h"

extern int g_stopEvent;
//构造函数
ngx_connection_s::ngx_connection_s()
{
    iCurrsequence = 0;
    pthread_mutex_init(&logicProcMutex,NULL);   
}

ngx_connection_s::~ngx_connection_s()
{
    pthread_mutex_destroy(&logicProcMutex);
}

//分配出去一个连接的时候初始化一些内容，原来内容放在ngx_get_connection()里，现在放在这里。
void ngx_connection_s::getOneToUse()
{
    ++iCurrsequence;
    curStat = _PKG_HD_INIT;                     //当前收包的状态
    precvbuf = dataHeadInfo;
    irecvlen = sizeof(COMM_PKG_HEADER);

    precvMemPointer = NULL;
    iThrowsendCount = 0;                        //原子的
    psendMemPointer = NULL;                     //发送数据头指针记录
    events = 0;                                 //epoll事件
}

void ngx_connection_s::putOneToFree()
{
    ++iCurrsequence;
    if( precvMemPointer != NULL ) 
    {
        CMemory::getInstance()->FreeMemory(precvMemPointer);
        precvMemPointer = NULL;
    }
    if( psendMemPointer != NULL )
    {
        CMemory::getInstance()->FreeMemory(psendMemPointer);
        psendMemPointer = NULL;
    }
    iThrowsendCount = 0;
}

void CSocket::initconnection()
{
    lpngx_connection_t p_Conn;
    CMemory* p_memory = CMemory::getInstance();
     
    int ilenconnpool = sizeof(ngx_connection_t);
    //先创建这么多个连接，后续不够的话在增加，m_worker_connections是配置中配置的epoll的最大连接数
    for( int i=0; i<m_worker_connections; ++i )
    {
        p_Conn = (lpngx_connection_t)p_memory->AllocMemory(ilenconnpool,true);
        p_Conn = new(p_Conn) ngx_connection_t();    //定位new，以p_Conn为首地址构造ngx_connection_t类型的对象
        p_Conn->getOneToUse();
        m_connectionList.push_back(p_Conn);
        m_freeconnectionList.push_back(p_Conn);
    }
    m_free_connection_n = m_total_connection_n = m_connectionList.size();

    return ;
}

//回收连接池，释放内存
void CSocket::clearConnection()
{
    lpngx_connection_t p_Conn;
    CMemory* p_memory = CMemory::getInstance();

    while( !m_connectionList.empty() )
    {
        p_Conn = m_connectionList.front();
        m_connectionList.pop_front();
        p_Conn->~ngx_connection_t();        //手动调用析构函数
        p_memory->FreeMemory(p_Conn);
    }
}

//从连接池中获取一个空闲连接，把这一个连接中的fd成员置为isock
lpngx_connection_t CSocket::ngx_get_connection(int isock)
{

#if 0
    lpngx_connection_t c = m_pfree_connections;         //空闲连接链表头
    
    if( c == NULL )
    {
        //系统应该能控制连接数量，防止空闲连接被耗尽，所以不应出现空闲链表为空的情况
        printf("CSocket::ngx_get_connection()中，空闲链表为空！\n");
        return NULL;
    }
    m_pfree_connections = c->data;                      //空闲链表头指针指向下一个未用的节点
    m_free_connection_n--;                              //空闲链表节点数量减少1

    //（1）先把c指向的对象中有用的东西搞出来保存成变量，因为这些数据可能会有用。
    uintptr_t instance = c->instance;
    uint64_t iCurrsequence = c->iCurrsequence;

    //（2）把以往有用的数据搞出来后，清空并进行适当的初始化
    memset(c,0,sizeof(ngx_connection_t));
    
    c->fd = isock;                                      //把套接字保存到c->fd
    c->curStat = _PKG_HD_INIT;                          //收包状态初始化为_PKG_HD_INIT，即准备接受数据包头
    
    c->precvbuf = c->dataHeadInfo;                      
    c->irecvlen = sizeof(COMM_PKG_HEADER);              //指定收数据的长度，这里先要要求收包头这么长字节的数据

    c->precvMemPointer = NULL;

    //（3）这个值有用，所以在上边（1）中被保留，没有被清空，这里又把这个值赋回来
    c->instance = !instance;                            //官方nginx就这么写的，暂时不知道有啥用
    c->iCurrsequence = iCurrsequence;                   
    ++c->iCurrsequence;                                 //每次取用该值都应该增加1

    return c;
#endif
    //因为可能有其他线程要访问m_freeconnectionList,m_connectionList这两个链表，所以需要互斥
    CLock lock(&m_connectionMutex);
    if( !m_freeconnectionList.empty() )
    {
        lpngx_connection_t p_Conn = m_freeconnectionList.front();
        m_freeconnectionList.pop_front();
        p_Conn->getOneToUse();
        --m_free_connection_n;
        p_Conn->fd = isock;
        return p_Conn;
    }

    //程序走到这里，表示没有空闲的连接了，那就再创建一个连接
    CMemory* p_memory = CMemory::getInstance();
    lpngx_connection_t p_Conn = (lpngx_connection_t)p_memory->AllocMemory(sizeof(ngx_connection_t),true);
    p_Conn = new(p_Conn) ngx_connection_t();
    p_Conn->getOneToUse();
    m_connectionList.push_back(p_Conn);     //入到总表中来
    ++m_total_connection_n;
    p_Conn->fd = isock;
    return p_Conn;

}

//归还参数c所代表的连接到连接池中，注意参数类型是lpngx_connection_t
void CSocket::ngx_free_connection(lpngx_connection_t c)
{
    
#if 0

    c->data = m_pfree_connections;          //回收的节点指向原来串起来的空闲链的链头

    ++c->iCurrsequence;
    
    m_pfree_connections = c;
    ++m_free_connection_n;                  //空闲链表节点数量+1
    return ;
#endif 
    CLock lock(&m_connectionMutex);
    c->putOneToFree();
    m_freeconnectionList.push_back(c);
    ++m_free_connection_n;

}

void CSocket::inRecyConnectQueue(lpngx_connection_t pConn)
{
    printf("CSocket::inRecyConnectQueue()执行，连接进去到回收队列中。\n");
    CLock lock(&m_recyconnqueueMutex);        //针对连接回收列表的互斥量，因为线程ServerRecyConnectionThread()也要用到这个回收列表。
    pConn->inRecyTime = time(NULL);             //记录回收时间
    ++pConn->iCurrsequence;
    m_recyconnectionList.push_back(pConn);      //等待ServerRecyConnectionThread线程自会处理
    ++m_totol_recyconnection_n;                 //延迟回收队列大小+1
    return ;
}

//处理连接回收的线程
void* CSocket::ServerRecyConnectionThread(void* threadData)
{
    ThreadItem* pThread = static_cast<ThreadItem*>(threadData);
    CSocket* pSocketObj = pThread->_pThis;
    time_t currtime;
    int err;
    std::list<lpngx_connection_t>::iterator pos, posend;
    lpngx_connection_t p_Conn;
    while(1)
    {
        //为简化问题，我们直接每次休息200毫秒
        usleep(200*1000);
        if( pSocketObj->m_totol_recyconnection_n > 0 )
        {
            currtime = time(NULL);
            err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
            if( err != 0 ) 
            {
                printf("CSocket::ServerRecyConnectionThread()中pthread_mutex_lock()失败，错误码为%d\n",err);
            }

lblRRTD:
            pos = pSocketObj->m_recyconnectionList.begin();
            posend = pSocketObj->m_recyconnectionList.end();
            for( ; pos!=posend; ++pos )
            {
                p_Conn = (*pos);
                if(  
                    ( ( p_Conn->inRecyTime + pSocketObj->m_RecyConnectionWaitTime ) > currtime ) && ( g_stopEvent == 0 )
                )
                {
                    continue;   //没到释放时间
                }

                //我认为凡是到释放时间的，iThrowsendCount都应该为0
                if( p_Conn->iThrowsendCount != 0 )
                {
                    printf("CSocket::ServerRecyConnectionThread()中到释放时间却发现p_Conn.iThrowsendCount!=0，这是不应该的！\n");
                }

                //流程走到这里，表示可以释放，开始释放
                --pSocketObj->m_totol_recyconnection_n;
                pSocketObj->m_recyconnectionList.erase(pos);
                printf("CSocket::ServerRecyConnectionThread()被执行，连接%d被归还\n",p_Conn->fd);
                pSocketObj->ngx_free_connection(p_Conn);
                goto lblRRTD;
            }
            //for循环结束之后，所有到释放时间的连接都释放了。
            err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
            if( err != 0 )
            {
                printf("CSocket::ServerRecyConnectionThread中，pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex)失败!\n");
            }
        }//end if( pSocketObj->m_totol_recyconnection_n > 0 )
        
        

    }//end while(1)

    return (void*)0;
}

//ngx_close_connection()给定一个连接的指针作为参数，收回这个连接，并关闭这个连接对应的套接字。
//1.用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放。
//2.recv()接收到的字节数为0时，认为客户端正常关闭了连接。
void CSocket::ngx_close_connection(lpngx_connection_t c)
{
    printf("调用了ngx_close_connection()\n");
    if( close(c->fd) == -1 )
    {
        printf("CSocket::ngx_close_connection()中close(%d)失败！\n",c->fd);
    }
    c->fd = -1;
    ngx_free_connection(c);
    printf("一个连接已被断开，调用了ngx_free_connection(c)\n");
    return ;
}

