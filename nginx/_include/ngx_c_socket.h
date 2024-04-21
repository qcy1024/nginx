#ifndef __NGX_C_SOCKET__H
#define __NGX_C_SOCKET__H

#include "ngx_global.h"
#include "ngx_c_comm.h"

#define NGX_LISTEN_BACKLOG 511  //已完成连接队列的最大值，listen函数的第二个参数
#define NGX_MAX_EVENTS     512  //epoll_wait一次最多接收这么多个事件

class CSocket;
struct ngx_connection_s;

typedef void (CSocket::*ngx_event_handler_pt)(struct ngx_connection_s* c);    //定义成员函数指针

//ngx_listening_t，监听类型，包含端口号和套接字描述符
typedef struct ngx_listening_s
{
    int port;                         //端口号
    int fd;                          //套接字描述符
    ngx_connection_s* connection;
}ngx_listening_t,*lpngx_listening_t;

//以下三个结构是非常重要的三个结构，这里遵从nginx官方的写法
//(1)该结构表示一个TCP连接【客户端主动发起的，nginx服务器被动接收的TCP连接】
typedef struct ngx_connection_s
{
    int                         fd;                             //对于服务器的监听套接字，该项是服务器对应的套接字描述符，对于与客户端
                                                                //通信的套接字，该项是客户端套接字描述符
    lpngx_listening_t           listening;                      //监听的对象指针,结构包含端口号、套接字描述符。对于服务端的套接字描述符来说，该字段
                                                                 //是服务器监听的端口号、套接字描述符以及connection信息；对于客户端的套接字描述符
                                                                 //来说，该字段是与该客户端套接字对应的服务端的套接字描述符的监听信息。

    unsigned                    instance:1;                      //位域。:1表示这个变量只占1位。
    struct sockaddr             s_sockaddr;


    uint8_t                     w_ready;                         //写准备好标记
    ngx_event_handler_pt        rhandler;                        //读事件的相关处理方法
    ngx_event_handler_pt        whandler;                        //写事件的相关处理方法

    //和收包有关
    unsigned char               curStat;                         //当前收包的状态
    char                        dataHeadInfo[_DATA_BUFSIZE_];    //用于保存收到的数据报的包头数据
    char*                       precvbuf;                        //接收数据的缓冲区的头指针，这个指针始终指向当前要接收数据的要存放的内存位置。对收到不全的包非常有用
    unsigned int                irecvlen;                        //(还)要接收多少数据，由这个变量指定，和precvbuf配套使用

    bool                        ifnewrecvMem;                    //如果我们成功收到了包头，那么我们就要分配内存开始保存消息头+包头+包体内容
                                                                 //这个标记用来标记是否new过内存，可以应对恶意包的内存释放问题。
    char*                       pnewMemPointer;                  //new出来的用于收包的内存首地址，和ifnewrecvMem配套使用


    ngx_connection_s*           data;               //这是个指针，等价与传统链表里的next成员，后继指针

    uint64_t                    iCurrsequence;      //我自己加的

}ngx_connection_t,*lpngx_connection_t;

//消息头，引入的目的是当收到数据包时，额外记录一些内容以备将来使用。
typedef struct _STRUC_MSG_HEADER
{
    ngx_connection_s* pConn;                    //记录对应的连接
    uint64_t          iCurrsequence;            //收到数据包时记录对应连接的序号，将来用于比较是否连接已经作废
    //......待拓展
}STRUC_MSG_HEADER,*LPSTRUC_MSG_HEADER;

class CSocket
{
public:
    CSocket();
    virtual ~CSocket();
public:
    virtual bool Initialize();      //初始化函数
    void ReadConf();       //读配置文件中与网络、epoll有关的配置项
    int ngx_epoll_init();  //初始化epoll，在ngx_worker_process_init()中被调用。
    int ngx_epoll_process_events(int timer);
    void ngx_event_accept(lpngx_connection_t oldc);    
    void ngx_free_connection(lpngx_connection_t c);
    void ngx_close_connection(lpngx_connection_t c);
    void ngx_process_events_and_timers();
    int ngx_epoll_add_event(int fd,
                                int readevent, int writeevent,
                                uint32_t otherflag,
                                uint32_t eventtype,
                                lpngx_connection_t c
                                );
    lpngx_connection_t ngx_get_connection(int isock);
    void ngx_wait_request_handler(lpngx_connection_t c);
    void ngx_wait_request_handler_proc_p1(lpngx_connection_t c);
    void ngx_wait_request_handler_proc_plast(lpngx_connection_t c);
    void inMsgRecvQueue(char* buf);
    void clearMsgRecvQueue();
    ssize_t recvproc(lpngx_connection_t c,char* buff,ssize_t buflen);

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

    int                              m_connection_n;        //当前进程可以连接的总数【连接池总大小】
    int                              m_free_connection_n;   //空闲链表中节点的数量

    std::vector<lpngx_listening_t>   m_ListenSocketList;  //监听套接字队列
    struct epoll_event               m_events[NGX_MAX_EVENTS];  

    //一些和网络通讯有关的成员变量
    size_t                           m_iLenPkgHeader;       //sizeof(COMM_PKG_HEADER);包头的大小
    size_t                           m_iLenMsgHeader;       //sizeof(STRUC_MSG_HEADER);消息头的大小

    //消息队列
    std::list<char*>                 m_MsgRecvQueue;        //接收数据消息队列
    int                              m_iRecvMsgQueueCount;  //收消息队列大小
    
    //多线程相关
    pthread_mutex_t                  m_recvMessageQueueMutex;   //收消息队列互斥量
};



#endif

