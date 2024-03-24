/*为若干信号注册了信号处理函数；编写了信号处理函数ngx_signal_handler*/

#include <string.h>
#include <stdlib.h>
#include "ngx_global.h"

typedef struct 
{
    int             signo;
    const char      *signame;
    //siginfo_t类型定义在signal.h头文件中
    //信号处理函数的返回类型和参数是固定的，操作系统就是这样要求的
    void (*handler) (int signo,siginfo_t *siginfo,void* ucontext);
} ngx_signal_t;

//信号处理函数
static void ngx_signal_handler(int signo,siginfo_t *siginfo,void* ucontext);

static void ngx_process_get_status(void);

ngx_signal_t signals[] = {
    //  signo       signame           handler
    {   SIGHUP,    "SIGHUP",          ngx_signal_handler },
    {   SIGINT,    "SIGINT",          ngx_signal_handler },
    {   SIGTERM,   "SIGTERM",         ngx_signal_handler },     //编号15
    {   SIGCHLD,   "SIGCHLD",         ngx_signal_handler },
    {   SIGQUIT,   "SIGQUIT",         ngx_signal_handler },
    {   SIGIO,     "SIGIO",           ngx_signal_handler },
    {   SIGSYS,    "SIGSYS,SIG_IGN",  NULL               },     //处理程序为NULL的，说明我们想忽略该信号。
    //...日后添加

    //哨兵
    {   0,         NULL,              NULL               }
};

//初始化信号的函数，用于注册信号处理程序,将signals[]里面定义的信号都注册
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
            //sa_flags被置为SA_SIGINFO表示sigaction指向的信号处理函数是一个复杂信号处理函数(接受3个参数的)
            sa.sa_flags = SA_SIGINFO;
        }
        else 
        {
            sa.sa_handler = SIG_IGN;
        }

        //sa_mask标志了在执行信号处理程序时屏蔽的信号集
        //这里把sa.sa_mask清0，即执行信号处理程序时阻塞的信号集为空，也就是说执行信号处理程序时其他的任何信号都可以打断。
        sigemptyset(&sa.sa_mask);

        //&sa中就包含了信号处理函数的地址
        if( sigaction(sig->signo,&sa,NULL) == -1 )
        {
            printf("ngx_init_signal中, sigaction(%s) failed\n",sig->signame);
            return -1;
        }
        
        else 
        {
            printf("signal %s register scceed!\n",sig->signame);
        }

    }
    return 1;
}

//信号处理函数
static void ngx_signal_handler(int signo,siginfo_t *siginfo,void* ucontext)
{
    ngx_signal_t* sig = signals;
    char* action;       //一个字符串，记录一个动作信息写日志文件。
    action = "";
    for( ; sig->signo!=0 && sig->signo!=signo; ++sig )
    {
        ;
    }
    if( sig->signo == 0 )
    {
        printf("收到了未注册的信号！\n");
        return ;
    }
    //master进程和worker进程接收到的信号可能是不一样的，也有不同的处理方式
    //如果是master进程：
    if( ngx_process == NGX_PROCESS_MASTER )
    {
        switch( signo )
        {
            case SIGCHLD:
            {
                ngx_reap = 1;   //标记子进程的状态变化，日后master主进程的for(;;)循环中可能会用到这个变量【比如产生一个新的子进程】
                break;
            }
            default: break;
        }
    }
    //如果是worker进程
    else if( ngx_process == NGX_PROCESS_WORKER )
    {

    }
    else 
    {

    }
    
    //记录来信号的日志信息
    if( siginfo && siginfo->si_pid )
    {
        printf("signal %d (%s) received from %d, action%s\n",signo,sig->signame,siginfo->si_pid,action);
    }
    else 
    {
        printf("signal %d (%s) received, action:%s\n",signo,sig->signame,action);
    }
    if( signo == SIGCHLD )
    {
        ngx_process_get_status();
    }
    
}

static void ngx_process_get_status(void)
{
    pid_t pid;
    int status;
    int err;
    int one = 0;   
    for(;;)
    {
        pid = waitpid(-1,&status,WNOHANG);
        if( pid == 0 )
        {
            return ;
        }
        else if( pid == -1 )
        {
            err = errno;
            if( err == EINTR )          //调用被某个信号中断
            {
                continue;
            }

            if( err == ECHILD && one )  //没有子进程
            {
                return ;
            }
            if( err == ECHILD )
            {
                printf("waitpid() failed!\n");
                return ;
            }
            printf("waitpid() failed!\n");
            return ;

        }
        //走到这里，表示waitpid()成功
        one = 1;
        if( WTERMSIG(status) )
        {
            printf("pid = %d exited on signal %d!\n",pid,WTERMSIG(status));
        }
        else 
        {
            printf("pid = %d exited with code %d\n",pid,WEXITSTATUS(status));
        }

    }//end of for(;;)
    return ;
}
