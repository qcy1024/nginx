#include "ngx_global.h"
#include "ngx_c_conf.h"
#include "ngx_c_socket.h"

CConfig *p_config;

//g_os_argv保存命令行参数argv[]
char** g_os_argv;
//新的保存environ[]的内存位置
char* gp_envmem = NULL;
//g_environlen保存environ[]的大小
int g_environlen = 0;

//socket相关
CSocket g_socket;       //socket全局对象

//和进程本身有关的全局量
pid_t ngx_pid;          //当前进程的pid
pid_t ngx_parent;       //当前进程的父进程的pid   

int g_daemonized = 0;   //标志进程是否启用了守护进程模式

int ngx_process;        //进程类型，比如worker进程，master进程等。
sig_atomic_t ngx_reap;  //sig_atomic_t是系统定义的一种原子类型。



int main(int argc,char* argv[],char* environ[])
{
    /*测试nginx_log_stderr函数*/
    //nginx_log_stderr(0,"333：%d, ,321: ,%d.,  一个字符串测试：%s, %d,,,... %s,,,...\n",333,321,"Hello,World!",456,"ttt");

    //将命令行参数指针保存在char**变量g_os_argv里面
    g_os_argv = (char**) argv;
    
    //初始化设置标题要做的事（把environ[]搬走）
    ngx_init_setproctitle(environ);
    //将进程的标题设置为mynginx!!!
    ngx_setproctitle("mynginx!!!");

    ngx_process = NGX_PROCESS_MASTER;

    //2）初始化失败就要直接退出的
    //获取一个单例类对象指针，用于读取并解析配置文件
    p_config = CConfig::getInstance();
    //CConfig的单例类对象p_config实际上就通过其成员m_ConfigItrmList保存的所有的配置文件条目。
    if( p_config->load("nginx.conf") == false )
    {
        ngx_log_stderr(0,"配置文件%s载入失败，退出！\n","nginx.conf");
        return -1;
    }
    

    /*读配置文件的测试*/
    // int i = 0;
    // for( std::vector<LPCConfItem>::iterator it = p_config->m_configItemList.begin(); it != p_config->m_configItemList.end(); ++it )
    // {

    //     printf("条目%d,名字是:%s, 内容是:%s\n",i++,(*it)->ItemName,(*it)->ItemContent );
    // }

    // /*测试CConfig::GetString以及CConfig::GetIntDefault*/
    // printf("条目%s，内容是%d\n条目%s，内容是%s\n","ListenPort",p_config->getIntDefault("ListenPort",10),
    //     "DBInfo",p_config->getString("DBInfo"));

    //3）一些初始化函数准备放这里
    ngx_log_init(p_config);
    if( ngx_init_signals() != 1 )
    {
        printf("信号初始化失败!\n");
        return -1;
    }

    //监听端口初始化
    if( g_socket.Initialize() == false )
    {
        printf("main中，g_socket.Initialize()==false成立了！\n");
        return 1;
    }

    //4)一些不好归类的其他代码放在这里


    //5)创建守护进程
    // if( p_config->getIntDefault("Daemon",0) == 1 )
    // {
    //     int ret = nginx_daemon();     //父进程返回1，子进程返回0，出错返回-1
    //     if( ret == -1 )
    //     {
    //         printf("main中，创建守护进程失败\n");
    //         return -1;
    //     }
    //     //父进程的分支,直接退出
    //     else if( ret == 1 )
    //     {
    //         return -1;
    //     }
    //     else if( ret == 0 )
    //     {
    //         g_daemonized = 1;
    //     }

    // }

    //6)正式开始的主工作流程，主流程一致在下边这个函数里循环，暂时不会走下来，资源释放什么的日后再慢慢完善和考虑
    ngx_master_process_cycle(); //不管是父进程还是子进程，正常工作期间都会在这个函数里死循环。

    for(;;)
    {
        printf("程序正在运行\n");
        sleep(1);
    }


    return 0;
}


