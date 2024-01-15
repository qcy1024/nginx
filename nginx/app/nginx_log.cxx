#include "ngx_global.h"
#include "ngx_macro.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>



//往标准错误(STDERR_FILENO)打印信息。
//nginx_log_stderr最终是使用write()函数向标准错误写信息：write(STDERR_FILENO,errstr,p-errstr)
void nginx_log_stderr(int err,const char* fmt,...)
{
    //nginx_log_stderr(0,"invalid option: %s , %d","testInfo",326);
    
    //最终要把处理好的格式串放在err_info里面
    char err_info[NGX_MAX_ERRORINFO_SIZE+1];
    memset(err_info,0,NGX_MAX_ERRORINFO_SIZE);
    
    char* end_pos = err_info + NGX_MAX_ERRORINFO_SIZE;
    
    va_list args;
    va_start(args,fmt);

    //调用函数ngx_fmt_process处理格式串，将fmt中的%d,%s等根据输入的可变参数替换，得到目标字符串，仍然存放在err_info里面。
    ngx_fmt_process(err_info,end_pos,fmt,args);

    //向标准错误STDERR_FILENO写入err_info的信息
    write(STDERR_FILENO,err_info,end_pos-err_info);

}


