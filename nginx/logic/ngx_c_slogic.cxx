#include "ngx_c_slogic.h"
#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_c_memory.h"
#include "ngx_c_crc32.h"
#include "ngx_c_conf.h"

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
    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;
    LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+m_iLenMsgHeader);
    void* pPkgBody = NULL;
    unsigned short pkgLen = ntohs(pPkgHeader->pkgLen);      //包头+包体的长度

    //这个包只有包头没有包体
    if( m_iLenPkgHeader == pkgLen )
    {
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

    //客户端发来的包，服务器解析处理期间，客户端断开连接了，就会使这个连接池中的iCurrsequence+1；
    if( p_Conn->iCurrsequence != pMsgHeader->iCurrsequence )
    {
        return ;
    }
    if( imsgCode >= AUTH_TOTAL_COMMANDS || statusHandler[imsgCode] == NULL )
    {
        printf("CLogicSocket::threadRecvProcFunc()中出现了未定义的消息码，消息码为%d\n",imsgCode);
        return ;
    }
    
    (this->*statusHandler[imsgCode])(p_Conn,pMsgHeader,(char*)pPkgBody,pkgLen-m_iLenPkgHeader);

    return ;
}


//处理各种业务逻辑的函数的实现
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength)
{
    printf("执行了_HandleRegister()函数!\n");
    return true;
}

bool CLogicSocket::_HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength)
{
    printf("执行了_HandleLogIn()函数!\n");
    return true;
}
