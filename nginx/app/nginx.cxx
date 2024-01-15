#include "ngx_global.h"
#include "ngx_c_conf.h"
#include <cstdio>
#include <unistd.h>

//g_os_argv保存命令行参数argv[]
char** g_os_argv;
//新的保存environ[]的内存位置
char* gp_envmem = NULL;
//g_environlen保存environ[]的大小
int g_environlen = 0;



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


    //获取一个单例类对象指针，用于读取并解析配置文件
    CConfig *p_config = CConfig::getInstance();
    
    //CConfig的单例类对象p_config实际上就通过其成员m_ConfigItrmList保存的所有的配置文件条目。
    if( p_config->load("nginx.conf") == false )
    {
        printf("配置文件载入失败，退出！\n");
        return 1;
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


    for(;;)
    {
        printf("程序正在运行\n");
        sleep(1);
    }


    return 0;
}

