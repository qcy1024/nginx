#ifndef __NGX_GLOBAL_H__ 
#define __NGX_GLOBAL_H__ 

typedef struct 
{
    char ItemName[50];
    char ItemContent[500];
}CConfItem,*LPCConfItem;

//外部全局量声明
extern void ngx_init_setproctitle(char**);
extern void ngx_setproctitle(const char*);
//g_os_argv保存命令行参数argv[]
extern char** g_os_argv;
//新的保存environ[]的内存位置
extern char* gp_envmem;
//g_environlen保存environ[]的大小
extern int g_environlen;


#endif