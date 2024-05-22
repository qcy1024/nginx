#ifndef __NGX_C_SLOGIC_H__ 
#define __NGX_C_SLOGIC_H__

#include "ngx_global.h"
#include "ngx_c_socket.h"

#define _CMD_REGISTER 5

class CLogicSocket : public CSocket
{
public:
    CLogicSocket();
    virtual ~CLogicSocket();
    virtual bool Initialize();

public:
    //业务逻辑相关函数
    bool _HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength);
    bool _HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char* pPkgBody,unsigned short iBodyLength);

public:
    virtual void threadRecvProcFunc(char* pMsgBuf);

};

#endif

