#include "ngx_c_socket.h"
#include "ngx_global.h"
#include "ngx_c_conf.h"
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

CSocket::CSocket()
{
    m_ListenPortCount = 1;
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
}

//初始化函数【】fork()子进程之前调用这个函数
//成功返回true，失败返回false
bool CSocket::Initialize()
{
    bool reco = ngx_open_listening_sockets();
    return reco;
}

//在配置文件指定的所有端口上开始监听
bool CSocket::ngx_open_listening_sockets()
{
    CConfig* p_config = CConfig::getInstance();
    m_ListenPortCount = p_config->getIntDefault("ListenPortCount",m_ListenPortCount);

    //printf("m_ListenPortCount是：%d\n",m_ListenPortCount);

    int isock;                          //socket
    struct sockaddr_in serv_addr;       
    int iport;                          //端口
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

        //ngx_listening_t是监听类型，包含成员fd:套接字描述符，以及port:监听的端口号。
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

