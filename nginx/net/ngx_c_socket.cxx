#include "ngx_c_socket.h"
#include "ngx_c_conf.h"
#include "ngx_c_memory.h"
#include "ngx_c_lockmutex.h"

#define EINTR 4
extern int g_stopEvent;

CSocket::CSocket()
{
    //配置相关
    m_worker_connections = 1;
    m_ListenPortCount = 1;

    m_epollhandle = -1;

    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);      //包头占用的字节数
    m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER);     //消息头占用的字节数
    
    //多线程相关
    //pthread_mutex_init()是POSIX线程库提供的函数。
    pthread_mutex_init(&m_recvMessageQueueMutex,NULL);  //互斥量初始化
    
    return ;
}

//析构函数
CSocket::~CSocket()
{
    std::vector<lpngx_listening_t>::iterator pos;
    for( pos=m_ListenSocketList.begin(); pos!=m_ListenSocketList.end(); ++pos )
    {
        delete (*pos);
    }
    m_ListenSocketList.clear();

    //多线程相关
    pthread_mutex_destroy(&m_recvMessageQueueMutex);        //互斥量释放

    return ;
}

// void CSocket::clearMsgRecvQueue()
// {
//     char* sTmpMempoint;
//     CMemory *p_memory = CMemory::getInstance();

//     while( !m_MsgRecvQueue.empty() )
//     {
//         sTmpMempoint = m_MsgRecvQueue.front();
//         m_MsgRecvQueue.pop_front();
//         p_memory->FreeMemory(sTmpMempoint);
//     }
//     return ;
// }

void CSocket::ReadConf()
{
    CConfig* p_config = CConfig::getInstance();
    //epoll最大连接数
    m_worker_connections = p_config->getIntDefault("worker_connections",1024);
    
    m_ListenPortCount = p_config->getIntDefault("ListenPortCount",1);
    //待回收连接等待时间
    m_RecyConnectionWaitTime = p_config->getIntDefault("Sock_RecyConnectionWaitTime",m_RecyConnectionWaitTime);
    return ;
}
 
//初始化函数【】fork()子进程之前调用这个函数
//成功返回true，失败返回false
bool CSocket::Initialize()
{
    ReadConf();
    bool reco = ngx_open_listening_sockets();
    return reco;
}

//子进程中才需要执行的初始化函数
bool CSocket::Initialize_subproc()
{
    //发消息互斥量初始化
    if( pthread_mutex_init(&m_sendMessageQueueMutex,NULL) != 0 )
    {
        printf("CSocket::Initialize_subproc()中pthread_mutex_init(&m_sendMessageQueueMutex,NULL)失败\n");
        return false;
    }
    //连接相关互斥量初始化
    if( pthread_mutex_init(&m_connectionMutex,NULL) != 0 )
    {
        printf("CSocket::Initialize_subproc()中pthread_mutex_init(&m_connectionMutex,NULL)失败\n");
        return false;
    }
    //连接回收队列相关互斥量初始化
    if( pthread_mutex_init(&m_recyconnqueueMutex,NULL) != 0 )
    {
        printf("CSocket::Initialize_subproc()中pthread_mutex_init(&m_recyconnqueueMutex,NULL)失败\n");
        return false;
    }

    if( sem_init(&m_semEventSendQueue,0,0) == -1 )
    {
        printf("CSocket::Initialize_subproc()中sem_init(&m_semEventSendQueue,0,0)失败!\n");
        return false;
    }

    //创建线程专门用来发送数据的线程
    int err;
    ThreadItem* pSendQueue;     
    m_threadVector.push_back(pSendQueue = new ThreadItem(this));
    err = pthread_create(&pSendQueue->_Handle,NULL,ServerSendQueueThread,pSendQueue);
    if( err != 0 )
    {
        return false;
    }
 
    //创建专门用来回收连接的线程
    ThreadItem* pRecyconn;      
    m_threadVector.push_back(pRecyconn = new ThreadItem(this));
    err = pthread_create(&pRecyconn->_Handle,NULL,ServerRecyConnectionThread,pRecyconn);
    if( err != 0 )
    {
        return false;
    }

    return true;
}


//在配置文件指定的所有端口上开始监听
bool CSocket::ngx_open_listening_sockets()
{
    //printf("m_ListenPortCount是：%d\n",m_ListenPortCount);

    int isock;                          //用于保存服务器端调用socket()返回的套接字描述符
    struct sockaddr_in serv_addr;       
    int iport;                          //用于保存配置文件中指定的要监听的端口号
    char strinfo[100];                  //临时字符串
    
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //INADDR_ANY表示一个服务器上的所有网卡，多个本地IP地址都进行绑定端口

    for( int i=0; i<m_ListenPortCount; ++i )
    {
        //socket的第三个参数0是固定用法
        isock = socket(AF_INET,SOCK_STREAM,0);
        if( isock == -1 )
        {
            printf("CSocket::ngx_open_listening_sockets中创建socket失败\n");
            return false;
        }

        //setsockopt()函数用于设置一些套接字选项。
        //参数2：表示级别，和参数3配套使用，也就是说，参数3如果确定了，参数2也就确定了。
        //参数3：允许重用本地地址。
        //设置SO_REUSEADDR，主要是为了解决TIME_WAIT状态下导致bind()失败的问题。
        int reuseaddr = 1;
        if( setsockopt(isock,SOL_SOCKET,SO_REUSEADDR,(const void*)&reuseaddr,sizeof(reuseaddr)) == -1 )
        {
            printf("CSocket::ngx_open_listening_sockets中setsockopt()失败\n");
            close(isock);
            return false;
        }

        //设置该socket为非阻塞，setnonblocking()函数在下面定义。
        if( setnonblocking(isock) == false )
        {
            printf("CSocket::ngx_open_listening_sockets中setnonblocking()失败\n");
            close(isock);
            return false;
        }

        strinfo[0] = 0;
        sprintf(strinfo,"ListenPort%d",i);
        
        iport = p_config->getIntDefault(strinfo,10000);
        
        //printf("iport 是 %d\n",iport);
        
        serv_addr.sin_port = htons((in_port_t)iport);   //头文件<netinet/in.h>中定义了in_port_t类型,实际上就是uint16_t

        if( bind(isock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1 )
        {
            printf("CSocket::ngx_open_listening_sockets中bind()失败\n");
            perror("错误信息：");
            close(isock);
            return false;
        }

        if( listen(isock,NGX_LISTEN_BACKLOG) == -1 )
        {
            printf("CSocket::ngx_open_listening_sockets中listen()失败\n");
            close(isock);
            return false;
        }

        lpngx_listening_t p_listensocketitem = new ngx_listening_t;
        memset(p_listensocketitem,0,sizeof(ngx_listening_t));
        p_listensocketitem->fd = isock;
        p_listensocketitem->port = iport;
        printf("监听%d端口成功！\n",iport);
        m_ListenSocketList.push_back(p_listensocketitem);

    }//end of for( int i=0; i<m_ListenPortCount; ++i )
    return true;
}

//设置socket连接为非阻塞模式
bool CSocket::setnonblocking(int sockfd)
{
    int nb = 1;     //0:清除，1:设置
    if( ioctl(sockfd,FIONBIO,&nb) == -1 )   //对sockfd进行一些控制，FIONBIO表示File IO Not Block IO,第三个参数1表示设置非阻塞，
    {
        return false;
    }
    return true;
}

//关闭监听端口
void CSocket::ngx_close_listening_sockets()
{
    for( int i=0; i<m_ListenPortCount; ++i )
    {
        close(m_ListenSocketList[i]->fd);
        printf("监听端口%d已关闭\n",m_ListenSocketList[i]->port);
    }
    return ;
}

//将一个要发送的消息放入发消息队列中
void CSocket::msgSend(char* p_sendbuf)
{
    CLock lock(&m_sendMessageQueueMutex);
    m_MsgSendQueue.push_back(p_sendbuf);
    ++m_iSendMsgQueueCount;                     //++才是原子操作

    //将信号量的值+1
    if( sem_post(&m_semEventSendQueue) == -1 )
    {
        printf("CSocket::msgSend中sem_post(&m_semEventSendQueue)失败!\n");
    }
    return ;
}

//这个函数创建epoll、创建连接池，遍历每一个正在监听的端口，将相应的套接字描述符加入到epoll对象中去，并添加读事件为关注事件
int CSocket::ngx_epoll_init()
{
    //（1）调用epoll_create(),创建epoll对象，将epoll对象的文件描述符赋给成员变量m_epollhandle
    m_epollhandle = epoll_create(m_worker_connections);     //m_worker_connections表示epoll连接的最大数量

    if( m_epollhandle == -1 )
    {
        printf("CSocket::ngx_epoll_init()中，epoll_create()失败！\n");
        exit(2);
    }

    //（2）创建连接池
    initconnection();
    
#if 0
    //m_connection_n表示当前进程可以连接的总数【连接池总大小】
    m_connection_n = m_worker_connections;                  //当前进程可以的连接置为epoll连接的最大数量，即连接池的大小。
    m_pconnections = new ngx_connection_t[m_connection_n];  //把连接池new出来，连接池数组中每一个元素是一个连接

    //对连接池进行初始化工作
    int i = m_connection_n;                       
    lpngx_connection_t next = NULL;
    lpngx_connection_t c = m_pconnections;      //连接池数组首地址

    //从后往前遍历连接池的每一项，这个do-while循环对连接池进行初始化。
    do
    {
        i--;
        c[i].data = next;
        c[i].fd = -1;
        c[i].instance = 1;                      //连接池中每一个连接的instance初始化为1
        c[i].iCurrsequence = 0;

        next = &c[i];
    } while(i);

    //初始化空闲链
    m_pfree_connections = next;                 //设置空闲连接链表头指针为m_pfree_connections
    m_free_connection_n = m_connection_n;       //空闲连接链表长度，刚刚初始化时没有连接，全是空闲的。
#endif
    
    //(3)遍历所有监听端口
    std::vector<lpngx_listening_t>::iterator pos;
    for( pos=m_ListenSocketList.begin(); pos!=m_ListenSocketList.end(); ++pos )
    {
        //从连接池中获取一个空闲连接对象，参数是监听套接字队列中的一项，表示一个服务器正在监听的端口的套接字描述符
        lpngx_connection_t p_Conn = ngx_get_connection((*pos)->fd);
        if( p_Conn == NULL )
        {
            //初始化的时候，连接池一定不是空的，如果空，说明有错误。
            printf("CSocket::ngx_close_listening_sockets()中ngx_get_connection失败!\n");
            exit(2);
        }

        //将监听对象和连接对象绑定
        p_Conn->listening = *pos;                //连接类型中的监听类型对象为该监听对象，即连接对象 和 监听对象 关联，方便通过连接对象找到监听对象
        (*pos)->connection = p_Conn;             //监听对象 和 连接对象 关联，方便通过监听对象找到连接对象             
        
        //对监听端口的读事件设置处理函数
        p_Conn->rhandler = &CSocket::ngx_event_accept;
        
        //ngx_epoll_add_event往socket上增加监听事件。
        //把每一个监听端口的套接字描述符及其关注的事件增加到epoll对象上
        // if( ngx_epoll_add_event((*pos)->fd,             //套接字描述符
        //                         1,0,                    //读，写（只关心读事件，所以读，写分别为1，0）
        //                         0,                      //其他补充标记
        //                         EPOLL_CTL_ADD,          //事件类型为增加
        //                         c                       //连接池中的连接
        //                        ) == -1 )
        // {
        //     exit(2);
        // }

        if( ngx_epoll_ope_event(
                                (*pos)->fd,
                                EPOLL_CTL_ADD,
                                EPOLLIN|EPOLLRDHUP,
                                0,                      //第2个参数为EPOLL_CTL_ADD时，这个参数无效
                                p_Conn
                                ) == -1 )
        {
            exit(2);
        }


        return 1;
    }
    return 1;
}

// //传入文件描述符以及一些信息
// int CSocket::ngx_epoll_add_event(int fd,
//                                 int readevent, int writeevent,
//                                 uint32_t otherflag,
//                                 uint32_t eventtype,
//                                 lpngx_connection_t c
//                                 )
// {
// //     struct epoll_event {
// //     uint32_t events; // 表示感兴趣的事件类型，可以是 EPOLLIN、EPOLLOUT、EPOLLERR、EPOLLHUP 等的按位或
// //     epoll_data_t data; // 用户数据，可以是文件描述符、指针等
// //     };
//     struct epoll_event ev;      //epoll_event是系统定义的类型
//     memset(&ev,0,sizeof(ev));

//     if( readevent == 1 )
//     {
//         //events字段是struct epoll_event中的一个成员，它是一个位掩码，可以用EPOLLIN,EPOLLOUT等宏来设置。
//         //EPOLLIN是读事件，也即是ready read；EPOLLRDHUP是客户端的关闭连接事件
//         ev.events = EPOLLIN|EPOLLRDHUP;     

//     }
//     else 
//     {

//     }

//     if( otherflag != 0 )
//     {
//         ev.events |= otherflag;
//     }

//     //data字段用于携带事件相关的用户数据。data是一个union，可以是一个指针或一个文件描述符（分别是ptr及fd）
//     //c->instance是那个位域。因为任何指针变量的最后一位一定是0，这个按位或运算就使得ptr保存了指针地址以及这个位域是1还是0的信息。
//     ev.data.ptr = (void*)( (uintptr_t)c | c->instance );        

//     //epoll_event对象ev的data字段附带了一个函数指针，目的是指明相应事件发生时的处理函数
//     if( epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1 )
//     {
//         printf("CSocket::ngx_epoll_add_event()中，epoll_ctl()失败！\n");
//         return -1;
//     }
//     return 1;
// }

//操作epoll上fd的事件
int CSocket::ngx_epoll_ope_event(int fd,
								uint32_t eventType,             //事件类型，如EPOLL_CTL_ADD, EPOLL_CTL_MOD等
								uint32_t flag,                  //标志，其含义取决于eventType
								int bcaction,                   //补充动作，用于补充flag标记的不足，0：增加，1：去掉 2：完全覆盖。只有eventType是EPOLL_CTL_MOD时这个参数有用。
								lpngx_connection_t pConn        
								)
{
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));
    if( eventType == EPOLL_CTL_ADD )
    {
        ev.data.ptr = (void*)pConn;
        ev.events = flag;
        pConn->events = flag;
    }
    else if( eventType == EPOLL_CTL_MOD )
    {
        ev.events = pConn->events;
        if( bcaction == 0 )
        {
            //增加某个标记
            ev.events |= flag;
        }
        else if( bcaction == 1 )
        {
            //去掉某个标记
            ev.events &= ~flag;
        }
        else 
        {
            //将events置为flag
            ev.events = flag;
        }
        pConn->events = ev.events;  //uint32_t
    }

    else 
    {
        return 1;
    }

    //当操作为EPOLL_MOD时，epoll_ctl的内核代码会无脑将epoll对象上的event置为新的ev参数，所以这里要将指针赋给ev.data.ptr
    ev.data.ptr = (void*)pConn;

    if( epoll_ctl(m_epollhandle,eventType,fd,&ev) == -1 )
    {
        printf("CSocket::ngx_epoll_ope_event()中，epoll_ctl(%d,%u,%u,%d)失败！\n",m_epollhandle,eventType,fd,ev.events);
    }
    return 1;
}

//epoll_wait()
int CSocket::ngx_epoll_process_events(int timer)
{

    //在m_epollhandle这个epoll对象上，最多接收NGX_MAX_EVENTS个事件，这些事件放在m_events为起始地址的内存中。
    //epoll_wait() 函数返回已经就绪的事件数，如果出现错误或超时，则返回 -1，并设置 errno 以指示错误类型
    //在服务器的监听套接字描述符上，“有连接已完成”是一个就绪事件。
    int events = epoll_wait(m_epollhandle,m_events,NGX_MAX_EVENTS,timer);

    printf("在ngx_epoll_process_events中，pid=%d,此时就绪的事件数量为：events = %d\n",getpid(),events);

    if( events == -1 )
    {
        //EINTR为宏定义，值为4。若errno为4，表示worker进程收到了信号，导致epoll_wait()失败
        if( errno == EINTR )
        {
            //一般认为信号所导致的epoll_wait()失败不是什么问题，直接返回。
            printf("CSocket::ngx_epoll_process_events()中，epoll_wait()失败！\n");
            return 1;   //正常返回
        }
        else 
        {
            printf("CSocket::ngx_epoll_process_events()中，epoll_wait()失败！\n");
            return 0;   //非正常返回
        }
    }

    else if( events == 0 )
    {
        //计时器不是-1，说明不是无限阻塞等待。这种情况说明等待事件到了，但是并没有事件就绪，已就绪的事件数为0.
        if( timer != -1 )
        {
            return 1;
        }
        //无限阻塞等待但却返回了0，这种情况是诡异的！
        else 
        {
            printf("CSocket::ngx_epoll_process_events()中epoll_wait()没超时，但是返回的已就绪事件数为0!\n");
            return 0;
        }
    }

    //events>0,表示有已就绪的事件
    else 
    {
        lpngx_connection_t c;
        uint32_t revents;

        //用一个for循环处理所有就绪事件
        for( int i=0; i<events; ++i )
        {
            //在
            c = (lpngx_connection_t)(m_events[i].data.ptr);
            revents = m_events[i].events;   //取出事件类型

            //这个事件是一个写事件
            if( revents & EPOLLOUT )
            {
                //EPOLLERR：对应的连接发生错误
                //EPOLLHUP：对应的连接被挂起
                //EPOLLRDHUP：表示TCP连接的远端关闭或者半关闭连接
                if( revents & ( EPOLLERR | EPOLLHUP | EPOLLRDHUP ) )
                {
                    --c->iThrowsendCount;
                }
                else 
                {
                    //执行读事件的处理函数rhandler,在ngx_epoll_init()中将rhandler设置成为了ngx_event_accept
                    ( this->* (c->whandler) ) (c) ;
                    printf("这是一个fd=%d上的写事件,调用了该fd上的写事件处理函数。\n",c->fd);
                }
            }

            //这个事件是一个读事件
            if( revents & EPOLLIN )
            {
                ( this->* (c->rhandler) ) (c) ;
                printf("这是一个fd=%d上的读事件,调用了该fd上的读事件处理函数。\n",c->fd); 
            }

        }//end of for( int i=0; i<events; ++i )
    }//end of else [events>0]

    return 1;
}
 
//数据发送线程
void* CSocket::ServerSendQueueThread(void* threadData)
{
    printf("执行了ServerSendQueueThread()\n");

    ThreadItem* pThread = static_cast<ThreadItem*>(threadData);
    CSocket* pSocketObj = pThread->_pThis;
    int err;
    std::list<char*>::iterator pos,pos2,posend;
    char* pMsgBuf;
    LPSTRUC_MSG_HEADER pMsgHeader;
    LPCOMM_PKG_HEADER pPkgHeader;
    lpngx_connection_t p_Conn;
    unsigned short itmp;
    ssize_t sendsize;

    CMemory* p_memory = CMemory::getInstance();

    while( 1 )
    {
        if( sem_wait(&pSocketObj->m_semEventSendQueue) == -1 )
        {
            //如果被某个信号中断，则sem_wait()也可能过早地返回，错误码errno为EINTR
            
            if( errno != EINTR )
            {
                printf("CSocket::ServerSendQueueThread()中sem_wait(&pSocketObj->m_semEventSendQueue)失败!\n");
            }
        }
        //%u是打印unsigned int
        printf("执行完sem_wait(&pSocketObj->m_semEventSendQueue)后，m_semEventSendQueue的值为%u\n",pSocketObj->m_semEventSendQueue);
        printf("此时发消息队列中的消息数量为%d\n",pSocketObj->m_iSendMsgQueueCount.load());
        if( pSocketObj->m_iSendMsgQueueCount > 0 )
        {
            err = pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex);
            if( err != 0 )
            {
                printf("CSocket::ServerSendQueueThread()中pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex)失败!\n");
            }
            pos = pSocketObj->m_MsgSendQueue.begin();
            posend = pSocketObj->m_MsgSendQueue.end();

            while( pos != posend )
            {
                pMsgBuf = (*pos);       //取出消息队列中的一个要发送的数据，每个都是消息头+包头+包体
                pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;
                pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+pSocketObj->m_iLenMsgHeader);
                p_Conn = pMsgHeader->pConn;

                //包过期
                if( p_Conn->iCurrsequence != pMsgHeader->iCurrsequence )
                {
                    pos2 = pos;
                    pos++;
                    pSocketObj->m_MsgSendQueue.erase(pos2);
                    --pSocketObj->m_iSendMsgQueueCount;
                    p_memory->FreeMemory(pMsgBuf);
                    continue;
                }

                if( p_Conn ->iThrowsendCount > 0 )  //iThrowsendCount在ngx_connection_s::getOneToUse()中初始化为0
                {
                    //靠系统驱动来发送消息，所以这里不能再发送。
                    pos++;
                    continue;
                }

                //走到这里，可以发送消息，一些必须的信息记录，要发送的东西也要从发送队列里移除
                p_Conn->psendMemPointer = pMsgBuf;
                pos2 = pos;
                pos++;
                pSocketObj->m_MsgSendQueue.erase(pos2);
                --pSocketObj->m_iSendMsgQueueCount;
                p_Conn->psendBuf = (char*)pPkgHeader;
                itmp = ntohs(pPkgHeader->pkgLen);
                p_Conn->isendLen = itmp;

                //%u用于打印unsigned int.
                printf("即将发送一个数据，长度为%u\n",p_Conn->isendLen);

                sendsize = pSocketObj->sendproc(p_Conn,p_Conn->psendBuf,p_Conn->isendLen);

                //可能全发送出去了，也可能只发送了一部分。
                if( sendsize > 0 )
                {
                    //成功发送的和要求发送的相等，说明全部发送成功了，
                    if( sendsize == p_Conn->isendLen )  //成功发送出去了数据，一下就发送出去了
                    {
                        //此时的psendMemPointer即pMsgBuf即*pos即要发送的数据。它是在业务逻辑处理函数里面，制作要回复给客户端的包的时候new出来的
                        p_memory->FreeMemory(p_Conn->psendMemPointer);
                        p_Conn->psendMemPointer = NULL;
                        p_Conn->iThrowsendCount = 0;
                        printf("CSocket::ServerSendQueueThread()中数据直接发送完了!\n");
                    }
                    //缓冲区满导致没有全部发送完毕(EAGAIN)
                    else 
                    {
                        //发送到了哪里，剩余多少，记录下来，方便下次sendproc()时使用。
                        p_Conn->psendBuf = p_Conn->psendBuf + sendsize;
                        p_Conn->isendLen = p_Conn->isendLen - sendsize;
                        //因为发送缓冲区满了，所以现在我们要等待可写事件，写事件就绪才能继续往缓冲区中发送消息
                        ++p_Conn->iThrowsendCount;
                        if( pSocketObj->ngx_epoll_ope_event(
                                                            p_Conn->fd,         
                                                            EPOLL_CTL_MOD,          
                                                            EPOLLOUT,       //增加EPOLLOUT(写事件)
                                                            0,              //对于事件类型为增加的，EPOLL_CTL_MOD需要这个参数，0：增加
                                                            p_Conn
                                                            ) == -1 )
                        {
                            printf("CSocket::ServerSendQueueThread()中ngx_epoll_ope_event()_1失败!\n");
                        }
                        printf("CSocket::ServerSendQueueThread()中因为缓冲区满，数据发送了一部分，没发送完毕。一共要发送:%d,实际发送了%ld\n",p_Conn->isendLen+sendsize,sendsize);
                    }
                    continue;
                }//end if( sendsize > 0 )

                //能走到这里的，应该是有点问题
                else if( sendsize == 0 )
                {
                    //sendsize==0一般表示超时
                    p_memory->FreeMemory(p_Conn->psendMemPointer);
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;
                    continue;
                }

                //发送缓冲区满
                else if( sendsize == -1 )
                {
                    ++p_Conn->iThrowsendCount;
                    //在fd上加入写事件
                    if( pSocketObj->ngx_epoll_ope_event(
                                                        p_Conn->fd,
                                                        EPOLL_CTL_MOD,
                                                        EPOLLOUT,
                                                        0,
                                                        p_Conn
                                                        ) == -1 )
                    {
                        printf("CSocket::ServerSendQueueThread()中ngx_epoll_ope_event()_2失败!\n");
                    }
                    printf("发送缓冲区满了，已在相应epoll上的fd上加入写事件。\n");
                    continue;
                }

                else    //if ( sendsize == -2 ) 
                {
                    //走到这里一般认为对端断开了，等待recv()来做断开socket以及回收资源
                    p_memory->FreeMemory(p_Conn->psendMemPointer);
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;
                    continue;
                }

            }//end while( pos != posend )

            err = pthread_mutex_unlock(&pSocketObj->m_sendMessageQueueMutex);
            if( err != 0 )
            {
                printf("CSocket::ServerSendQueueThread()中, pthread_mutex_unlock()失败！\n");
            }

        }//end if( pSocketObj->m_iSendMsgQueueCount > 0 )

    }//end while(1)
    return (void*)0;
}   

