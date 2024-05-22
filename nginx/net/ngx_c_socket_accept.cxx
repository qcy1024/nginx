#include "ngx_c_conf.h"
#include "ngx_c_socket.h"
#include "ngx_macro.h"

void CSocket::ngx_process_events_and_timers()
{
    ngx_epoll_process_events(-1);
}
 

//建立新连接专用函数，当新连接进入时，本函数会被ngx_epoll_process_events()所调用
//在ngx_epoll_init()中，所有监听套接字的读事件处理函数被设置成了该函数。
void CSocket::ngx_event_accept(lpngx_connection_t oldc)
{
    struct sockaddr         mysockaddr;             //用于存储客户端地址信息
    socklen_t               socklen;
    int                     s;                      //用于保存客户端套接字描述符，后面用accept4的返回值为其赋值
    static int              use_accept4 = 1;        //先认为能够使用accept4()函数
    lpngx_connection_t      newc;

    int                     err;
    int                     level;

    socklen = sizeof(mysockaddr);

    do
    {
        if( use_accept4 )
        {
            // accept4() 是 accept() 的扩展，引入了一些额外的选项。
            // 可以通过设置 flags 参数来指定一些标志，以更精确地控制接受连接的行为。常见的 flags 包括：
            // SOCK_NONBLOCK：非阻塞模式，使得返回的连接套接字是非阻塞的。如果不指定 flags 参数，accept4() 的行为与 accept() 完全相同
            s = accept4(oldc->fd,&mysockaddr,&socklen,SOCK_NONBLOCK);
        }  
        else 
        {
            //accept三个参数分别是：服务端套接字描述符、用于存储客户端地址信息的指针、长度
            s = accept(oldc->fd,&mysockaddr,&socklen);
        }
        
        if( s == -1 )
        {
            err = errno;
            //EAGAIN在error.h中定义
            if( err == EAGAIN )
            {
                return ;
            }
            level = NGX_LOG_ALERT;
            if( err == ECONNABORTED )
            {
                level = NGX_LOG_ERR;
            }
            //EMFILE：进程的fd已用尽【已达到系统所允许单一进程所能打开的最大文件数】
            else if( err == EMFILE || err == ENFILE )   
            {
                level = NGX_LOG_CRIT;
            }
            printf("CSocket::ngx_event_accept()中accept4()失败！\n");

            //ENOSYS表示系统不支持accept4()函数
            if( use_accept4 && err == ENOSYS )
            {
                use_accept4 = 0;
                continue;
            }

            //对方关闭套接字
            if( err == ECONNABORTED )
            {
                //对方关闭套接字了，什么也不用干
            }

        }//end of if( s == -1 )

        //走到这里，就表示accept4()/accept()成功了，即取到了一个已完成的连接

        //从空闲连接池取出一个空闲连接，将该连接的fd字段置为s(客户端的套接字描述符)
        newc = ngx_get_connection(s);
        if( newc == NULL )
        {
            if( close(s) == -1 )
            {
                printf("CSocket::ngx_event_accept()中close(%d)失败！\n",s);
            }
            return ;
        }

        //将来在这里判断是否超过最大允许的连接数

        //成功的拿到了连接池中的一个连接
        memcpy(&newc->s_sockaddr,&mysockaddr,socklen);

        if( !use_accept4 )
        {
            if( setnonblocking(s) == false )
            {
                ngx_close_connection(newc);
                return ;
            }
        }

        newc->listening = oldc->listening;
        newc->w_ready = 1;

        //设置读写事件就绪时的处理函数
        newc->rhandler = &CSocket::ngx_read_request_handler;    //recvproc(), inMsgQueue()
        newc->whandler = &CSocket::ngx_write_request_handler;   //写事件中调用sendproc()发送数据

        //把客户端的套接字描述符加到epoll对象上来
        //在ngx_epoll_add_event()函数中需要把指针newc传进去的原因是，要在epoll事件中记录相应的事件处理函数(epoll的事件类型events就保存了
        //该事件的处理函数)。
        // if( ngx_epoll_add_event(s,
        //                         1,0,
        //                         //EPOLLET,        //这个标记将epoll设置为边缘触发
        //                         0,
        //                         EPOLL_CTL_ADD,
        //                         newc ) == -1 )
        // {
        //     ngx_close_connection(newc);
        //     return ;
        // }

        if( ngx_epoll_ope_event(
                                s,                      //socket
                                EPOLL_CTL_ADD,          
                                EPOLLIN|EPOLLRDHUP,     //设置关注事件为EPOLLIN(可读)以及EPOLLRDHUP(TCP连接断开)
                                0,                      //对于EPOLL_CTL_ADD，不需要这个参数
                                newc                    //连接
                                ) == -1 )
        {
            ngx_close_connection(newc);
            return ;
        }

        break;
    } while (1);

}



