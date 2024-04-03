#include "ngx_c_conf.h"
#include "ngx_c_socket.h"


//来数据时候的处理，当连接上有数据来的时候，本函数会被ngx_epoll_process_events()所调用，
void CSocket::ngx_wait_request_handler(lpngx_connection_t c)
{
    printf("2222222222");
    return ;
}




