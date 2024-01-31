#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "ngx_c_conf.h"
#include "ngx_global.h"

static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int inum,const char* pprocname);
static void ngx_worker_process_cycle(int inum,const char* pprocname);
static void ngx_worker_process_init(int inum);

//main函数中，程序会在这个函数中死循环。
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
    sigaddset(&set,SIGTERM);
    sigaddset(&set,SIGQUIT);

    if( sigprocmask(SIG_BLOCK,&set,NULL) == -1 )
    {
        printf("ngx_master_process_cycle()中sigprocmask()失败\n");
    }

    CConfig* p_config = CConfig::getInstance();
    int workprocess = p_config->getIntDefault("WorkerNum",1);
    //调用这个函数来创建子进程
    ngx_start_worker_processes(workprocess);

    //只有父进程master的流程才会走到这里，子进程会在ngx_start_worker_processes里面循环。
    ngx_setproctitle("master process");
    sigemptyset(&set);

    //父进程master会一直在这个死循环里干活。
    for(;;)
    {
        printf("这里是父进程\n");
        //sigsuspend()函数根据指定的信号集&set设置新的阻塞信号集，并使进程阻塞在sigsuspend()处。
        sigsuspend(&set);
        
    }
}

//根据给定的参数创建指定数量的子进程，因为以后可能要扩展功能，增加参数，所以单独写成一个函数
//threadnums:要创建的子进程数量。 
static void ngx_start_worker_processes(int threadnums)
{
    int i;
    for( i=0; i<threadnums; i++ )
    {
        ngx_spawn_process(i,"worker process");
    }
    return ;
}

static int ngx_spawn_process(int inum,const char* pprocname)
{
    pid_t pid;
    pid = fork();
    switch(pid)
    {
        case -1: 
            printf("ngx_spawn_process中，创建子进程失败\n");
            return -1;
            break;
        case 0: //子进程分支
            ngx_parent = ngx_pid;
            ngx_pid = getpid();
            //子进程worker会在这个函数中死循环干活。
            ngx_worker_process_cycle(inum,pprocname);
            break;
        default:
            break;
    }//end switch(pid)
    //父进程分支才会走到这里，子进程的流程在case 0 中就死循环了。


    return pid;
}


static void ngx_worker_process_cycle(int inum,const char* pprocname)
{
    //做一些创建子进程的初始化工作，将来可能再新加一些内容，所以将初始化工作写成一个函数
    ngx_worker_process_init(inum);
    ngx_setproctitle(pprocname);


    for(;;)
    {
        printf("这里是子进程%d\n",inum);
        sleep(1);
    }
}

static void ngx_worker_process_init(int inum)
{
    sigset_t set;
    sigemptyset(&set);
    //标志SIG_SETMASK：将blocked设置为set。这里就是不再阻塞任何信号。
    if( sigprocmask(SIG_SETMASK,&set,NULL) == -1 )
    {
        printf("ngx_worker_process_init()中sigprocmask()失败!\n");
    }
}

