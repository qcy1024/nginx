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

typedef struct ngx_connection_s
{

    ngx_connection_s();
    ~ngx_connection_s();
    void putOneToFree();
    void getOneToUse();

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

    char*                       precvMemPointer;                  //new出来的用于收包的内存首地址

    pthread_mutex_t             logicProcMutex;                  //逻辑处理相关的互斥量(针对于每一个连接)


    //和发包有关
    //消息发送线程本是该类的CSocket::ServerSendQueueThread()函数来实现，但若缓冲区满，数据没有发送完毕，就要把没发送完毕的部分扔出去，
    //通过epoll写事件驱动，每扔出去一个，变量iThrowsendCount就要+1.
    std::atomic<int>            iThrowsendCount;                //发送消息，如果发送缓冲区满了，则需要通过epoll事件来驱动消息的继续发送
                                                                //在ngx_connection_s::getOneToUse()中初始化为0
    char*                       psendMemPointer;                //发送数据缓冲区的指针，指向消息头+包头+包体
    char*                       psendBuf;                       //发送数据的缓冲区指针，指向包头+包体
    unsigned int                isendLen;                       //要发送多少数据

    //和回收有关
    time_t                      inRecyTime;                     //入到待回收队列里去的时间

    //----------------------------------------
    ngx_connection_s*           data;               //这是个指针，等价与传统链表里的next成员，后继指针
    uint64_t                    iCurrsequence;      //

    uint32_t                    events;             //该连接上关心的事件集合

}ngx_connection_t,*lpngx_connection_t;


//消息头，引入的目的是当收到数据包时，额外记录一些内容以备将来使用。
typedef struct _STRUC_MSG_HEADER
{
    ngx_connection_s* pConn;                    //记录对应的连接
    uint64_t          iCurrsequence;            //收到数据包时记录对应连接的序号，将来用于比较是否连接已经作废
    
}STRUC_MSG_HEADER,*LPSTRUC_MSG_HEADER;


class CSocket
{
public:
    CSocket();
    virtual ~CSocket();
    virtual bool Initialize();      //初始化函数

public:
    virtual void threadRecvProcFunc(char* pMsgBuf);

public:
    void ReadConf();       //读配置文件中与网络、epoll有关的配置项
    int ngx_epoll_init();  //初始化epoll，在ngx_worker_process_init()中被调用。
    void initconnection();  //初始化连接池，在ngx_epoll_init()中被调用。
    int ngx_epoll_process_events(int timer);    
    void ngx_event_accept(lpngx_connection_t oldc);    //监听套接字的读事件处理函数
    void ngx_free_connection(lpngx_connection_t c);
    void ngx_close_connection(lpngx_connection_t c);
    void clearConnection();
    void ngx_process_events_and_timers();
    // int ngx_epoll_add_event(int fd,
    //                             int readevent, int writeevent,
    //                             uint32_t otherflag,
    //                             uint32_t eventtype,
    //                             lpngx_connection_t c
    //                             );
    int ngx_epoll_ope_event(int fd,
								uint32_t eventType,             //事件类型，如EPOLL_CTL_ADD, EPOLL_CTL_MOD等
								uint32_t flag,                  //标志，其含义取决于eventType
								int bcaction,                   //补充动作，用于补充flag标记的不足，0：增加，1：去掉
								lpngx_connection_t pConn        
								);
    lpngx_connection_t ngx_get_connection(int isock);
    void ngx_read_request_handler(lpngx_connection_t c);    //epoll对象上与客户端连接对应的套接字描述符上面的读事件的处理函数。
    void ngx_wait_request_handler_proc_p1(lpngx_connection_t c);
    void ngx_wait_request_handler_proc_plast(lpngx_connection_t c);
    void ngx_write_request_handler(lpngx_connection_t pConn);   //写事件处理函数
    
    ssize_t recvproc(lpngx_connection_t c,char* buff,ssize_t buflen);   //recv()的封装


    void inRecyConnectQueue(lpngx_connection_t pConn);                  //将要回收的连接放到m_recyconnectionList中来。

    //线程相关函数
    static void* ServerSendQueueThread(void* threadData);               //专门用来发送数据的线程
    static void* ServerRecyConnectionThread(void* threadData);          //专门用来回收连接的线程


    bool Initialize_subproc();
    void msgSend(char* p_sendbuf);
    ssize_t sendproc(lpngx_connection_t c,char* buff,ssize_t size);
    

public:
    //一个线程条目
    struct ThreadItem 
    {
        pthread_t _Handle;          //线程句柄，即线程标识符
        CSocket* _pThis;            //记录该线程所属的CSocket类对象的指针
        bool ifrunning;             //标记是否正式启动起来

        ThreadItem(CSocket* pthis):_pThis(pthis),ifrunning(false) {};
        ~ThreadItem() {};
    };
    //std::list<char*> getMsgRecvQueue(){ return m_MsgRecvQueue; };

private:
    bool ngx_open_listening_sockets();      //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets();     //关闭监听套接字
    bool setnonblocking(int sockfd);        //设置非阻塞套接字

protected:
    //一些和网络通讯有关的成员变量
    size_t                           m_iLenPkgHeader;       //sizeof(COMM_PKG_HEADER);包头的大小
    size_t                           m_iLenMsgHeader;       //sizeof(STRUC_MSG_HEADER);消息头的大小


    int                              m_worker_connections;   //epoll连接的最大数量
    int                              m_ListenPortCount;      //所监听的端口数量
    int                              m_epollhandle;          //epoll_create返回的句柄

    //和连接池有关的
    std::list<lpngx_connection_t>    m_connectionList;          //连接池
    std::list<lpngx_connection_t>    m_freeconnectionList;      //空闲连接列表
    std::atomic<int>                 m_total_connection_n;      //连接池的总连接数
    std::atomic<int>                 m_free_connection_n;       //连接池的空闲连接数
    pthread_mutex_t                  m_connectionMutex;         //连接相关互斥量
    pthread_mutex_t                  m_recyconnqueueMutex;      //连接回收队列相关互斥量
    std::list<lpngx_connection_t>    m_recyconnectionList;      //将要释放的连接放这里(延迟回收链表)
    std::atomic<int>                 m_totol_recyconnection_n;  //待释放连接队列大小
    int                              m_RecyConnectionWaitTime;  //等待这些秒之后回收连接


    std::vector<lpngx_listening_t>   m_ListenSocketList;        //监听套接字队列
    struct epoll_event               m_events[NGX_MAX_EVENTS];  


    //消息队列
    std::list<char*>                 m_MsgSendQueue;            //发送消息队列
    std::atomic<int>                 m_iSendMsgQueueCount;      //发送消息队列大小

    //多线程相关
    //pthread_mutex_t是POSIX线程库中定义的互斥锁类型
    std::vector<ThreadItem*>         m_threadVector;            //线程的容器
    pthread_mutex_t                  m_sendMessageQueueMutex;   //发送消息队列互斥量
    sem_t                            m_semEventSendQueue;       //处理发消息线程相关的信号量

    pthread_mutex_t                  m_recvMessageQueueMutex;   //接收消息队列互斥量




    

};



#endif

