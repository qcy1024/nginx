#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_c_conf.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

//全局变量ngx_log中有两个成员：log_fd以及log_level
ngx_log_t ngx_log;

//往标准错误(STDERR_FILENO)打印信息。
//nginx_log_stderr最终是使用write()函数向标准错误写信息：write(STDERR_FILENO,errstr,p-errstr)
void ngx_log_stderr(int err,const char* fmt,...)
{
    va_list args;
    va_start(args,fmt);
    //最终要把处理好的格式串放在err_info里面
    char err_info[NGX_MAX_ERRORINFO_SIZE+1];
    memset(err_info,0,NGX_MAX_ERRORINFO_SIZE);
    
    char* end_pos = err_info + NGX_MAX_ERRORINFO_SIZE;

    //调用函数ngx_fmt_process处理格式串，将fmt中的%d,%s等根据输入的可变参数替换，得到目标字符串，仍然存放在err_info里面。
    ngx_fmt_process(err_info,end_pos,fmt,args);

    //向标准错误STDERR_FILENO写入err_info的信息
    write(STDERR_FILENO,err_info,end_pos-err_info);
}

//日志初始化函数，打开/创建日志文件，置全局变量ngx_log的值。
void ngx_log_init(CConfig* p_config)
{
    const char* log_file_name = p_config->getString("LogFileName");
    
    if( log_file_name == NULL )
    {
        printf("log_file_name的内容是NULL\n");
        log_file_name = NGX_DEFAULT_LOG_PATH;
    }
    ngx_log.log_level = p_config->getIntDefault("LogLevel",6);
    //如果文件原来没内容，就以只写方式打开；如果文件原来有内容，就以追加方式打开；如果文件不存在，就创建文件。
    ngx_log.log_fd = open(log_file_name,O_WRONLY|O_APPEND|O_CREAT,0644);
    if( ngx_log.log_fd < 0 )
    {
        //这里给%s一个参数为字符串常量如"error.log"就没问题，给一个char*类型的局部变量就会有bug。
        //这个问题不只是我的ngx_log_stderr会出现，就算直接用printf，也有同样的问题。
        //以上bug在把Rtrim以及Ltrim中，加入除换行符'\r'之后，神奇地消失了！！！！！
        //明白了：之前因为log_file_name结尾有换行符，在ngx_fmt_process中把换行符也存进去了，导致输出的显示bug。
        ngx_log_stderr(0,"日志文件%s载入失败，退出！\n",log_file_name);
        ngx_log.log_fd = STDERR_FILENO;
    }
}


//
void ngx_logfile_print(int level,int err,const char* fmt,...)
{
    va_list args;
    va_start(args,fmt);
    char err_info[NGX_MAX_ERRORINFO_SIZE+1];
    char* end_pos = err_info + NGX_MAX_ERRORINFO_SIZE + 1;
    memset(err_info,0,NGX_MAX_ERRORINFO_SIZE);
    char* iter_err_info = err_info;
    char str_level[12];
    int_to_string(level,str_level,12);
    strcpy(iter_err_info,"error level:");
    iter_err_info += strlen("error level:");
    strcpy(iter_err_info,str_level);
    iter_err_info += strlen(str_level);
    *iter_err_info++ = ',';
    ngx_fmt_process(iter_err_info,end_pos,fmt,args);
    write(STDERR_FILENO,err_info,end_pos-err_info);
}