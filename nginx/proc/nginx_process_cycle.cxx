
#include "ngx_c_conf.h"
#include "ngx_global.h"
#include "ngx_c_socket.h"
#include "ngx_c_threadpool.h"

extern CSocket g_socket;       //socket全局对象

extern CThreadPool g_threadpool;   //线程池全局对象
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
    int workprocess = p_config->getIntDefault("WorkerProcesses",1);
    //调用这个函数来创建子进程
    ngx_start_worker_processes(workprocess);

    //只有父进程master的流程才会走到这里，子进程会在ngx_start_worker_processes里面循环。
    ngx_setproctitle("master process");
    sigemptyset(&set);

    //父进程master会一直在这个死循环里干活。
    for(;;)
    {
        printf("这里是父进程\n");
        //sigsuspend()是master进程的for循环里用到的一个主要函数。sigsuspend()函数根据指定的信号集&set设置新的阻塞信号集，并使进程阻塞在
        //sigsuspend()处，等待一个信号。此时进程是挂起状态，不占cpu时间，只有收到信号才会被唤醒。所以，此时master进程完全靠信号驱动干活。
        
        //收到一个信号s1后，就回去执行s1的信号处理程序，执行完之后，sigsuspend()返回，并将进程的信号屏蔽集设置为调用suspend()之前，
        //然后程序继续往下走。
        //sigsuspend()一定程度上是用来取代sigprocmask()。因为sigprocmask()不能处理
        //这种情况：正在屏蔽某些信号的时候，来了该信号。而sigsuspend()是原子操作，就可以处理这种情况。实际上，sigsuspend()的一系列操作
        //中，就包含了对sigprocmask()的调用。
        //ngx_worker_process_init中通过sigemptyset()取消了子进程对信号的屏蔽，父进程则在这里通过sigsuspend()取消对信号的屏蔽。
        sigsuspend(&set);   //这个函数其中一部分功能有点向sigprocmask(SIG_SETMASK,&set,NULL)
        printf("执行到suspend()下边来了\n");

    }
}

//根据给定的参数创建指定数量的子进程
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

//子进程的工作循环函数
static void ngx_worker_process_cycle(int inum,const char* pprocname)
{
    ngx_setproctitle(pprocname);
    ngx_worker_process_init(inum);
    
    ngx_process = NGX_PROCESS_WORKER;

    for(;;)
    {
        // printf("这里是子进程%d\n",inum);
        // sleep(1);
        g_socket.ngx_process_events_and_timers();            //处理网络事件和定时器事件
    }

    g_threadpool.StopAll();
}

//worker进程的初始化函数
static void ngx_worker_process_init(int inum)
{
    sigset_t set;
    sigemptyset(&set);
    //标志SIG_SETMASK：将blocked设置为set。这里就是不再阻塞任何信号。
    //由父进程fork出来的子进程，与父进程的内存映像是完全一样的，因此，其阻塞的信号集与父进程也是一样的；
    //我们在之前创建进程之前屏蔽了好多信号，在子进程创建好了之后，将这些信号恢复。
    if( sigprocmask(SIG_SETMASK,&set,NULL) == -1 )
    {
        printf("ngx_worker_process_init()中sigprocmask()失败!\n");
    }

    //要在epoll初始化之前就把线程池创建好，epoll一初始化可能就来事件了。
    CConfig* p_config = CConfig::getInstance();
    int tmpthreadnums = p_config->getIntDefault("ProcMsgRecvWorkThreadCount",5);
    //Create()保证所有创建出的线程都阻塞在pthread_cond_wait()处后才会返回。
    if( g_threadpool.Create(tmpthreadnums) == false )
    {
        exit(2);
    }

    if( g_socket.Initialize_subproc() == false )
    {
        exit(-2);
    }

    g_socket.ngx_epoll_init();

    return ;
}

