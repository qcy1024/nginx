#ifndef __NGX_C_SOCKET__H
#define __NGX_C_SOCKET__H
#include <vector>
#include "ngx_global.h"

#define NGX_LISTEN_BACKLOG 511  //已完成连接队列的最大值，listen函数的第二个参数

//ngx_listening_t，监听类型，包含端口号和套接字描述符
typedef struct ngx_listening_s
{
    int port;
    int fd;
}ngx_listening_t,*lpngx_listening_t;

class CSocket
{
public:
    CSocket();
    virtual ~CSocket();
public:
    virtual bool Initialize();      //初始化函数

private:
    bool ngx_open_listening_sockets();      //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets();     //关闭监听套接字
    bool setnonblocking(int sockfd);        //设置非阻塞套接字

private:
    int m_ListenPortCount;      //所监听的端口数量
    std::vector<lpngx_listening_t> m_ListenSocketList;  //监听套接字队列

};

#endif