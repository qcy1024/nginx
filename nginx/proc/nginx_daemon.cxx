
#include "ngx_global.h"


int nginx_daemon()
{
    //此处fork出来的子进程才是nginx中的master进程。
    pid_t pid = fork();
    if( pid > 0 )
    {
        return 1;   //父进程直接返回1
    }
    else if( pid < 0 )
    {
        return -1;
    }
    if( setsid() < 0 )
    {
        printf("nginx_daemon中，setsid()失败\n");
        return -1;
    }

    ngx_parent = ngx_pid;   //ngx_parent始终保存当前进程的父进程的pid
    ngx_pid = getpid();     //ngx_pid始终保存当前进程的pid

    umask(0);
    
    //为了调试，下面的将标准输入输出定位到空设备的代码先注释掉

    // int fd = open("/dev/null",O_RDWR);
    // if( fd == -1 )
    // {
    //     return -1;
    // }
    // if( dup2(fd,STDIN_FILENO) < 0 )
    // {
    //     return -1;
    // }
    // if( dup2(fd,STDOUT_FILENO) < 0 )
    // {
    //     return -1;
    // }
    // if( fd > STDERR_FILENO )
    // {
    //     if( close(fd) == -1 )
    //     {
    //         return -1;
    //     }
    // }
    //子进程返回0
    return 0;
}