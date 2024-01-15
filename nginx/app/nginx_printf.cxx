#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "ngx_global.h"
#include "ngx_macro.h"

//根据可变参数args,填充格式串fmt，将完整的格式串写入以bg_pos起始的位置中去，最多写到end_pos的前一个位置，即end_pos是尾后迭代器
void ngx_fmt_process(char* bg_pos,char* end_pos,const char* fmt, va_list args )
{
    if( end_pos <= bg_pos ) return ;
    char* iter_dst = bg_pos;
    const char* iter_fmt = fmt;

    int tmpInt = 0;
    char* tmpstr = NULL;

    while( *iter_fmt != '\0' && iter_dst < end_pos )
    {
        if( *iter_fmt == '%' )
        {
            iter_fmt++;
            switch( *iter_fmt )
            {
                case 'd': {
                    //用va_arg()来获取va_list类型的args中的下一个参数，以int类型获取
                    tmpInt = va_arg(args,int);
                    char tmpstr[12];
                    int_to_string(tmpInt,tmpstr,12);
                    if( iter_dst + strlen(tmpstr) < end_pos )
                    {
                        strcpy(iter_dst,tmpstr);
                        iter_dst += strlen(tmpstr);
                    }
                    else {
                        printf("nginx_printf.cxx中ngx_fmt_process出错，格式串太长了，给定的空间存不下\n");
                        return ;
                    }
                }; break;

                case 's': {
                    //这个可变参中的字符串参数怎么获取，目前我是用va_arg()获取char*类型的值，通过判断
                    //它的长度，来决定它的长度是否合法，如果合法，就将其拷贝进目标空间iter_dst中。
                    //注意，char* tmpstr = va_arg(args,char*)这一条语句会使strlen(tmpstr)为返回的格式串的strlen。
                    char* tmpstr = va_arg(args,char*);
                    if( strlen(tmpstr) >= MAX_FMT_STRLEN )
                    {
                        return ;
                    }
                    if( iter_dst + strlen(tmpstr) < end_pos )
                    {
                        strcpy(iter_dst,tmpstr);
                        iter_dst += strlen(tmpstr);
                    }
                    else {
                        return ;
                    }
                }
            }
            iter_fmt++;
        }
        else 
        {
            *iter_dst = *iter_fmt;
            iter_dst++;
            iter_fmt++;
        }
    }
    
    /*测试代码*/
    //printf("in ngx_fmt_process: 处理好的格式串为：%s\n",bg_pos);

}