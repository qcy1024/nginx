#include "ngx_global.h"


//初始化工作，将原本的environ[]的内容搬走到我们自己申请的内存，并令environ[i]指向对应的新的内存(environ数组的内容是不变的)
void ngx_init_setproctitle(char** environ)
{
    for( int i=0; environ[i] != NULL; i++ )
    {
        //计算environ[]的总大小
        g_environlen += strlen(environ[i]) + 1;
    }
    //申请environ[]总大小那么大的内存
    gp_envmem = new char[g_environlen];
    memset(gp_envmem,0,g_environlen);

    char* ptmp = gp_envmem;

    //把原来的environ[]内容搬到新的地方来，并令environ[i]指向新的内存
    for( int i=0; environ[i] != NULL; i++ )
    {
        size_t size = strlen(environ[i]) + 1;
        strcpy(ptmp,environ[i]);
        environ[i] = ptmp;
        ptmp += size;
    }
    return ;

}


//将程序的名字设置成指定的title
//我们假设所有的命令行参数我们都不需要用到了，并且标题的长度不会超过argv[]和envp[]的总内存长度
void ngx_setproctitle(const char* title)
{
    //计算新标题的长度
    size_t ititlelen = strlen(title);

    //计算原始的总的argv[]加envp[]的长度
    size_t e_environlen = 0;
    for( int i=0; g_os_argv[i] != NULL; i++ )
    {
        e_environlen += strlen(g_os_argv[i]) + 1;
    }
    size_t esy = e_environlen + g_environlen;
    if( esy <= ititlelen )
    {
        printf("ngx_setproctitle中的ngx_setproctitle出错，给定标题的长度太长了！\n");
        return ;
    }

    g_os_argv[1] = NULL;
    char* ptmp = g_os_argv[0];
    strcpy(ptmp,title);

    //printf("ngx_setproctitle()中，将argv[0]处的内存修改为了%s,标题长度为%lu\n",ptmp,ititlelen);
    
    ptmp += ititlelen;

    size_t cha = esy - ititlelen;
    memset(ptmp,0,cha);

    return ;
}