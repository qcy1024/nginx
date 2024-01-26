#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

typedef struct 
{
    int             signo;
    const char      *signame;
    //siginfo_t类型定义在signal.h头文件中
    //信号处理函数的返回类型和参数是固定的，操作系统就是这样要求的
    void (*handler) (int signo,siginfo_t *siginfo,void* ucontext);
} ngx_signal_t;

static void ngx_signal_handler(int signo,siginfo_t *siginfo,void* ucontext);

ngx_signal_t signals[] = {
    //  signo       signame           handler
    {   SIGHUP,    "SIGHUP",          ngx_signal_handler },
    {   SIGINT,    "SIGINT",          ngx_signal_handler },
    {   SIGTERM,   "SIGTERM",         ngx_signal_handler },
    {   SIGCHLD,   "SIGCHLD",         ngx_signal_handler },
    {   SIGQUIT,   "SIGQUIT",         ngx_signal_handler },
    {   SIGIO,     "SIGIO",           ngx_signal_handler },
    {   SIGSYS,    "SIGSYS,SIG_IGN",  NULL               },
    //...日后添加

    //哨兵
    {   0,         NULL,              NULL}
};

//初始化信号的函数，用于注册信号处理程序
int ngx_init_signals()
{
    ngx_signal_t *sig;
    struct sigaction sa;

    for( sig = signals; sig->signo != 0; sig++ )
    {
        memset(&sa,0,sizeof(struct sigaction));
        if( sig->handler )
        {
            sa.sa_sigaction = sig->handler;
            sa.sa_flags = SA_SIGINFO;
        }
        else 
        {
            sa.sa_handler = SIG_IGN;
        }
        sigemptyset(&sa.sa_mask);

        
        if( sigaction(sig->signo,&sa,NULL) == -1 )
        {
            printf("sigaction(%s) failed\n",sig->signame);
            return -1;
        }
        
        else 
        {
            printf("signal %s scceed!\n",sig->signame);
        }

    }
    return 1;
}

static void ngx_signal_handler(int signo,siginfo_t *siginfo,void* ucontext)
{
    printf("来信号了\n");
}