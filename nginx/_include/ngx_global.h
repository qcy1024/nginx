#ifndef __NGX_GLOBAL_H__ 
#define __NGX_GLOBAL_H__ 
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

#define NGX_PROCESS_MASTER 0    //master进程
#define NGX_PROCESS_WORKER 1    //worker进程

typedef struct 
{
    char ItemName[50];
    char ItemContent[500];
}CConfItem,*LPCConfItem;

typedef struct {
    int log_fd;
    int log_level;
}ngx_log_t;





/*外部全局量声明*/

//g_os_argv保存命令行参数argv[]
extern char** g_os_argv;
//新的保存environ[]的内存位置
extern char* gp_envmem;
//g_environlen保存environ[]的大小
extern int g_environlen;

class CConfig;

extern ngx_log_t ngx_log;

extern pid_t ngx_pid;          //当前进程的pid
extern pid_t ngx_parent;       //父进程的pid   

extern int g_daemonized;   //标志进程是否启用了守护进程模式

extern int ngx_process;        //进程类型，比如worker进程，master进程等。

extern sig_atomic_t ngx_reap;  //sig_atomic_t是系统定义的一种原子类型。


/*函数声明*/


extern void ngx_init_setproctitle(char**);
extern void ngx_setproctitle(const char*);


void ngx_fmt_process(char* bg_pos,char* end_pos,const char* fmt, va_list args );
void ngx_log_stderr(int err,const char* fmt,...);
void int_to_string(int num,char* str,int max_len);
void Ltrim(char*);
void Rtrim(char*);

void ngx_log_init(CConfig* p_config);
void ngx_logfile_print(int level,int err,const char* fmt,...);

int ngx_init_signals();

void ngx_master_process_cycle();

int nginx_daemon();

#endif