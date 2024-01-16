/*各种宏定义存放在这个头文件里*/

#ifndef __NGX_MACRO_H__
#define __NGX_MACRO_H__

//错误信息最长为2048字符
#define NGX_MAX_ERRORINFO_SIZE 2048
//格式串%s的最大长度
#define MAX_FMT_STRLEN 512

#define ngx_cpymem(dst,src,n)  ( ( (u_char *) memcpy(dst,src,n) ) + (n) )



//日志相关
#define NGX_LOG_STDERR      0       //最高级别的错误，直接写到标准错误
#define NGX_LOG_EMERG       1       //紧急【emerg】
#define NGX_LOG_ALERT       2       //警戒【alert】
#define NGX_LOG_CRIT        3       //严重【crit】
#define NGX_LOG_ERR         4       //错误【error】
#define NGX_LOG_WARN        5       //警告【warn】
#define NGX_LOG_NOTICE      6       //注意【notice】
#define NGX_LOG_INFO        7       //信息【info】
#define NGX_LOG_DEBUG       8       //调试【debug】

#define NGX_DEFAULT_LOG_PATH  "logs/error1.log"   //存放日志的路径

#endif
