#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

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
    {   SIGTERM,   "SIGTERM",         ngx_signal_handler },     //编号15
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

    //这个循环就是把signals[]这个数组中的所有信号都注册为它们对应的信号处理程序(信号处理程序在
    //数组中定义，这里都是ngx_signal_handler)
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
        //这里把sa.sa_mask清0，即执行信号处理程序时阻塞的信号集为空，也就是说执行信号处理程序时其他的任何信号都可以打断。
        sigemptyset(&sa.sa_mask);

        //&sa中就包含了信号处理函数的地址
        if( sigaction(sig->signo,&sa,NULL) == -1 )
        {
            printf("sigaction(%s) failed\n",sig->signame);
            return -1;
        }
        
        else 
        {
            printf("signal %s register scceed!\n",sig->signame);
        }

    }
    return 1;
}

static void ngx_signal_handler(int signo,siginfo_t *siginfo,void* ucontext)
{
    printf("来信号了\n");
    if( signo == SIGINT )
    {
        printf("来自键盘的中断，退出！\n");
        exit(0);
    }
}