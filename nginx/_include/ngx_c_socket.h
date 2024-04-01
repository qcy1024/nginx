#ifndef __NGX_C_SOCKET__H
#define __NGX_C_SOCKET__H
#include <vector>
#include "ngx_global.h"
#include <sys/epoll.h>

#define NGX_LISTEN_BACKLOG 511  //已完成连接队列的最大值，listen函数的第二个参数
#define NGX_MAX_EVENTS     512  //epoll_wait一次最多接收这么多个事件

typedef void (CSocket::*ngx_event_handler_pt)(lpngx_connection_t c);    //定义成员函数指针

//ngx_listening_t，监听类型，包含端口号和套接字描述符
typedef struct ngx_listening_s
{
    int port;       //端口号
    int fd;         //套接字描述符
}ngx_listening_t,*lpngx_listening_t;

//以下三个结构是非常重要的三个结构，这里遵从nginx官方的写法
//(1)该结构表示一个TCP连接【客户端主动发起的，nginx服务器被动接收的TCP连接】
typedef struct ngx_connection_s
{
    int                         fd;                 
    lpngx_listening_t           listening;          //监听的对象指针,结构包含端口号、套接字描述符

    unsigned                    instance:1;         //位域。:1表示这个变量只占1位。
    uint64_t                    iCurrsequence;
    struct sockaddr             s_sockaddr;

    uint8_t                     w_ready;

    ngx_event_handler_pt        rhandler;           //读事件的相关处理方法
    ngx_event_handler_pt        whandler;           //写事件的相关处理方法

    ngx_connections_s*          data;               //这是个指针，等价与传统链表里的next成员，后继指针

}ngx_connection_t,*lpngx_connection_t;



class CSocket
{
public:
    CSocket();
    virtual ~CSocket();
public:
    virtual bool Initialize();      //初始化函数
    void CSocket::ReadConf();       //读配置文件中与网络、epoll有关的配置项
    int CSocket::ngx_epoll_init();  //

    void CSocket::ngx_event_accept(lpngx_connection_t oldc);    
    int CSocket::ngx_epoll_add_event(int fd,
                                int readevent, int writeevent,
                                uint32_t otherflag,
                                uint32_t eventtype,
                                lpngx_connection_t c
                                );

private:
    bool ngx_open_listening_sockets();      //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets();     //关闭监听套接字
    bool setnonblocking(int sockfd);        //设置非阻塞套接字

private:
    int                              m_worker_connections;   //epoll连接的最大数量
    int                              m_ListenPortCount;      //所监听的端口数量
    int                              m_epollhandle;          //epoll_create返回的句柄

    //和连接池有关的
    //连接池就是一个数组,是一个元素类型为ngx_connection_t类型的数组。
    //引入这个数组就是为了把每一个套接字跟一块内存绑定在一起

    //连接池数组首地址，在CSocket::ngx_epoll_init()中用new初始化：m_pconnections = new ngx_connection_t[m_connection_n]; 
    lpngx_connection_t               m_pconnections;         
    //空闲连接链表表头。连接池中总是有某些连接被占用，空闲链表为了快速在池中找到一个空闲的连接(我们需要一个空闲项时，把空闲链表表头取出来就行)。
    lpngx_connection_t               m_pfree_connections;    

    int                              m_connection_n;        //当前进程可以连接进程的总数【连接池总大小】
    int                              m_free_connection_n;   //空闲链表中节点的数量


    std::vector<lpngx_listening_t>   m_ListenSocketList;  //监听套接字队列

    struct epoll_event               m_events[NGX_MAX_EVENTS];  

};

#endif