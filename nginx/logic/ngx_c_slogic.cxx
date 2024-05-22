#include "ngx_c_slogic.h"
#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_c_memory.h"
#include "ngx_c_crc32.h"
#include "ngx_c_conf.h"
#include "ngx_logiccomm.h"
#include "ngx_c_lockmutex.h"

typedef bool (CLogicSocket::*handler)(   lpngx_connection_t pConn,           //指向连接池中连接的指针
                                        LPSTRUC_MSG_HEADER pMsgHeader,      //消息头指针
                                        char* pPkgBody,                     //包体指针
                                        unsigned short iBodyLength );       //包体长度

static const handler statusHandler[] = 
{

    NULL,                                                               //0
    NULL,                                                               //1
    NULL,                                                               //2
    NULL,                                                               //3
    NULL,                                                               //4

    &CLogicSocket::_HandleRegister,                                     //5，注册功能
    &CLogicSocket::_HandleLogIn,                                        //6，登录功能

};

#define AUTH_TOTAL_COMMANDS sizeof(statusHandler)/sizeof(handler)     //算出statusHandler[]数组的条目数


CLogicSocket::CLogicSocket()
{

}

CLogicSocket::~CLogicSocket()
{

}

//初始化函数【fork()子进程之前干这个事】
//成功返回true，失败返回false
bool CLogicSocket::Initialize()
{
    bool bParentInit = CSocket::Initialize();   //调用父类的同名函数
    return bParentInit;
}

//处理收到的数据包
void CLogicSocket::threadRecvProcFunc(char* pMsgBuf)
{
    //printf("线程tid=%lu被激发了，开始工作\n",pthread_self());
    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;
    LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+m_iLenMsgHeader);
    
    void* pPkgBody = NULL;
    unsigned short pkgLen = ntohs(pPkgHeader->pkgLen);      //包头+包体的长度

    //这个包只有包头没有包体
    if( m_iLenPkgHeader == pkgLen )
    {
        //printf("线程tid=%lu收到了一个只有包头没有包体的数据包，将它丢弃，不做处理。\n",pthread_self());
        if( pPkgHeader->crc32 != 0 )
        {
            return ;
        }
        pPkgBody = NULL;
    }
    else 
    {
        pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);
        pPkgBody = (void*)(pMsgBuf+m_iLenMsgHeader+m_iLenPkgHeader);
        int calccrc = CCRC32::getInstance()->getCRC((unsigned char* )pPkgBody,pkgLen-m_iLenPkgHeader);
        if( calccrc != pPkgHeader->crc32 )
        {
            printf("CLogicSocket::threadRecvProcFunc中出现了CRC错误，丢弃数据\n");
            return ;
        }
    }
    
    unsigned short imsgCode = ntohs(pPkgHeader->msgCode);
    lpngx_connection_t p_Conn = pMsgHeader->pConn;

    // //客户端发来的包，服务器解析处理期间，客户端断开连接了，就会使这个连接池中的iCurrsequence+1；
    // if( p_Conn->iCurrsequence != pMsgHeader->iCurrsequence )
    // {
    //     return ;
    // }
    
    if( imsgCode >= AUTH_TOTAL_COMMANDS || statusHandler[imsgCode] == NULL )
    {
        printf("CLogicSocket::threadRecvProcFunc()中出现了未定义的消息码，消息码为%d\n",imsgCode);
        return ;
    }
    
    (this->*statusHandler[imsgCode])(p_Conn,pMsgHeader,(char*)pPkgBody,pkgLen-m_iLenPkgHeader);

    return ;
}



/*处理各种业务逻辑的函数的实现*/

//连接池指针，消息头指针，包体指针，服务端实际收到的包的包体长度
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength)
{
    printf("执行了_HandleRegister()函数!\n");
    if( pPkgBody == NULL )
    {
        return false;
    }

    int iRecvLen = sizeof(STRUCT_REGISTER);
    //服务器收到的包体长度应该就是协议中STRUCT_REGISTER的长度，如果不等，认为这个包有问题。
    if( iRecvLen != iBodyLength )
    {
        return false;
    }

    //对每一个连接的客户端指令进行互斥
    CLock lock(&pConn->logicProcMutex);
    //取得整个包体的内容
    LPSTRUCT_REGISTER p_RecvInfo = (LPSTRUCT_REGISTER)pPkgBody;
    
    LPCOMM_PKG_HEADER pPkgHeader;
    CMemory* p_memory = CMemory::getInstance();
    CCRC32* p_crc32 = CCRC32::getInstance();
    int iSendLen = sizeof(STRUCT_REGISTER);

//iSendLen = 65000;   //测试缓冲区满时，通过写就绪事件发送数据
    //分配要发送出去的包的内存
    char* p_sendbuf = (char*)p_memory->AllocMemory(m_iLenMsgHeader + m_iLenPkgHeader+iSendLen,false);
    //填充消息头
    memcpy(p_sendbuf,pMsgHeader,m_iLenMsgHeader);
    //填充包头
    pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf+m_iLenMsgHeader);
    pPkgHeader->msgCode = _CMD_REGISTER;
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
    pPkgHeader->pkgLen = htons(m_iLenPkgHeader+iSendLen);
    //填充包体
    LPSTRUCT_REGISTER p_sendInfo = (LPSTRUCT_REGISTER)(p_sendbuf+m_iLenMsgHeader+m_iLenPkgHeader);
    //这里根据需要，填充要发挥客户端的内容，int类型使用htonl转，short类型使用htons转

    //包体内容全部填充好之后，计算包体的crc32值
    pPkgHeader->crc32 = p_crc32->getCRC((unsigned char*)p_sendInfo,iSendLen);
    pPkgHeader->crc32 = htonl(pPkgHeader->crc32);

    //发送数据包
    msgSend(p_sendbuf);
    // if( ngx_epoll_ope_event(
    //                         pConn->fd,
    //                         EPOLL_CTL_MOD,
    //                         EPOLLOUT,       //写事件
    //                         0,
    //                         pConn
    //                         ) == -1 )
    // {
    //     printf("11111111111111111111111111111111\n");
    // }

    return true;
}

bool CLogicSocket::_HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength)
{
    printf("执行了_HandleLogIn()函数!\n");
    return true;
}
