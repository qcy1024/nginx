#include <stdio.h>
#include <unistd.h>


void ngx_master_process_cycle()
{
    sigset_t set;
    sigemptyset(&set);
    //要阻塞一系列信号，在执行下面的代码创建进程时，不希望受到信号的干扰
    sigaddset(&set,SIGCHLD);
    sigaddset(&set,SIGALRM);
    sigaddset(&set,SIGIO);
    sigaddset(&set,SIGINT);
    sigaddset(&set,SIGHUP);
    sigaddset(&set,SIGUSR1);
    sigaddset(&set,SIGUSR2);
    sigaddset(&set,SIGWINCH);
    sigaddset(&set,SIGCTERM);
    sigaddset(&set,SIGQUIT);

    if( sigprocmask(SIG_BLOCK,&set,NULL) == -1 )
    {
        printf("ngx_master_process_cycle()中sigprocmask()失败\n");
    }



}