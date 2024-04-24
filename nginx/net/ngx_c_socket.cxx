#include "ngx_c_socket.h"
#include "ngx_c_conf.h"
#include "ngx_c_memory.h"

#define EINTR 4

CSocket::CSocket()
{
    //配置相关
    m_worker_connections = 1;
    m_ListenPortCount = 1;

    //epoll相关
    m_epollhandle = -1;
    m_pconnections = NULL;
    m_pfree_connections = NULL;

    //一些和网络通讯有关的常用变量值，供后续频繁使用时提高效率
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);      //包头占用的字节数
    m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER);     //消息头占用的字节数

    //m_iRecvMsgQueueCount = 0;                       //收消息队列
    
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

    //释放连接池
    if( m_pconnections != NULL )
    {
        delete [] m_pconnections;
    }

    //释放接收消息队列的内容
    //clearMsgRecvQueue();

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
    m_worker_connections = p_config->getIntDefault("worker_connections",1024);
    m_ListenPortCount = p_config->getIntDefault("ListenPortCount",1);
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

//ngx_epoll_init()在worker进程的初始化函数中被调用。
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

    
    //(3)遍历所有监听端口
    std::vector<lpngx_listening_t>::iterator pos;
    for( pos=m_ListenSocketList.begin(); pos!=m_ListenSocketList.end(); ++pos )
    {
        //从连接池中获取一个空闲连接对象，参数是监听套接字队列中的一项，表示一个服务器正在监听的端口的套接字描述符
        c = ngx_get_connection((*pos)->fd);
        if( c == NULL )
        {
            //初始化的时候，连接池一定不是空的，如果空，说明有错误。
            printf("CSocket::ngx_close_listening_sockets()中ngx_get_connection失败!\n");
            exit(2);
        }

        //将监听对象和连接对象绑定
        c->listening = *pos;                //连接类型中的监听类型对象为该监听对象，即连接对象 和 监听对象 关联，方便通过连接对象找到监听对象
        (*pos)->connection = c;             //监听对象 和 连接对象 关联，方便通过监听对象找到连接对象

        //rev->accept = 1;                 
        
        //对监听端口的读事件设置处理函数，因为监听端口是用来等待客户端连接发送三次握手的，所以监听端口关心的就是读事件。
        c->rhandler = &CSocket::ngx_event_accept;
        
        //ngx_epoll_add_event往socket上增加监听事件。
        //把每一个监听端口的套接字描述符及其关注的事件增加到epoll对象上
        if( ngx_epoll_add_event((*pos)->fd,             //套接字描述符
                                1,0,                    //读，写（只关心读事件，所以读，写分别为1，0）
                                0,                      //其他补充标记
                                EPOLL_CTL_ADD,          //事件类型为增加
                                c                       //连接池中的连接
                               ) == -1 )
        {
            exit(2);
        }
        return 1;


    }

}

//传入文件描述符以及一些信息
int CSocket::ngx_epoll_add_event(int fd,
                                int readevent, int writeevent,
                                uint32_t otherflag,
                                uint32_t eventtype,
                                lpngx_connection_t c
                                )
{
//     struct epoll_event {
//     uint32_t events; // 表示感兴趣的事件类型，可以是 EPOLLIN、EPOLLOUT、EPOLLERR、EPOLLHUP 等的按位或
//     epoll_data_t data; // 用户数据，可以是文件描述符、指针等
//     };
    struct epoll_event ev;      //epoll_event是系统定义的类型
    memset(&ev,0,sizeof(ev));

    if( readevent == 1 )
    {
        //events字段是struct epoll_event中的一个成员，它是一个位掩码，可以用EPOLLIN,EPOLLOUT等宏来设置。
        //EPOLLIN是读事件，也即是ready read；EPOLLRDHUP是客户端的关闭连接事件
        ev.events = EPOLLIN|EPOLLRDHUP;     

    }
    else 
    {

    }

    if( otherflag != 0 )
    {
        ev.events |= otherflag;
    }

    //data字段用于携带事件相关的用户数据。data是一个union，可以是一个指针或一个文件描述符（分别是ptr及fd）
    //c->instance是那个位域。因为任何指针变量的最后一位一定是0，这个按位或运算就使得ptr保存了指针地址以及这个位域是1还是0的信息。
    ev.data.ptr = (void*)( (uintptr_t)c | c->instance );        

    //epoll_event对象ev的data字段附带了一个函数指针，目的是指明相应事件发生时的处理函数
    if( epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1 )
    {
        printf("CSocket::ngx_epoll_add_event()中，epoll_ctl()失败！\n");
        return -1;
    }
    return 1;
}

//将套接字描述符加入epoll对象(红黑树)时，(注意：这棵红黑树中的每一个节点包含文件描述符fd及事件event的信息。)
//同时传入了一个ngx_connection_t*类型的指针，指向连接池中的一项,这个指针用event.data.ptr保存。到时候调用
//epoll_wait()从红黑树中取事件的时候，就可以通过这个事件中的event.data.ptr指针，取出对应的连接池中的那一项。
//这一项就包含了很多信息。如：事件对应的文件描述符、读写事件处理函数等。于是，便可以知道事件用什么函数处理。

//这个函数在worker进程干活的死循环for()中被调用。
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
        uintptr_t instance;         //uintptr_t类型是一个无符号整数类型，可以在整数和指针类型之间相互转换。
        uint32_t revents;

        //用一个for循环处理所有就绪事件
        for( int i=0; i<events; ++i )
        {
            c = (lpngx_connection_t)(m_events[i].data.ptr);
            instance = (uintptr_t)c & 1;    //得到instance是0还是1
            c = (lpngx_connection_t)( (uintptr_t)c & (uintptr_t) ~ 1 );     //得到c真正的地址(末位为0)
            
            //以下两个if处理过期事件，
            if( c->fd == -1 )
            {
                //c->fd什么情况会为-1?  假如我们用epoll_wait()取得三个事件，第一个事件和第三个事件对应的是同一个连接，
                //处理第一个事件时，因为业务需要，将连接关闭了，此时就会调用ngx_close_connected_connedtion()函数，将
                //该连接中的fd置为-1。那么，在处理第三个事件时，c->fd就会为-1.
                printf("CSocket::ngx_epoll_process_events()中遇到了fd为-1的过期事件\n");
                continue;       //这种事件不处理即可
            }

            //instance是当时ngx_epoll_add_event()时加进去的instance的值，而c->instance是此时此刻收到事件的时候，instance的值
            //这两个值是有可能会不一样的。
            if( c->instance != instance )
            {
                printf("CSocket::ngx_epoll_process_events()中遇到了instance值改变的过期事件\n");
                continue;       //这种事件不处理即可
            }

            //能走到这里，就认为这个事件没过期，可以正常处理。
            revents = m_events[i].events;   //取出事件类型
            if( revents & ( EPOLLERR|EPOLLHUP ) )   //如果发生了错误或者客户端断开了连接，这里会感应到
            {
                revents |= EPOLLIN|EPOLLOUT;    //EPOLLIN:表示对应的连接上有数据可以读出
                                                //EPOLLOUT:表示对应的连接上可以写入数据发送
            }
            
            //个人思考：这里的过期判断存在漏洞。在ngx_connection_t中引入了一个iCurrsequence，试图解决这个问题。

            //如果这个事件是一个读事件
            if( revents & EPOLLIN )
            {
                //printf("数据来了~~~~~\n");
                //执行读事件的处理函数rhandler,在ngx_epoll_init()中将rhandler设置成为了ngx_event_accept
                (this->* (c->rhandler) ) (c) ;

            }

            //如果这个事件是一个写事件
            if( revents & EPOLLOUT )
            {
                //printf("这是一个写事件\n");
                
            }

        }//end of for( int i=0; i<events; ++i )
    }//end of else [events>0]

    return 1;
}

