#ifndef __NGINX_STRING__CXX
#define __NGINX_STRING__CXX

/*一些和字符串处理相关的函数放在这个文件里面*/
#include <string.h>


//输入字符串source，除掉其左侧无效字符(空格、回车、制表符)
void Ltrim(char* s)
{
    char* pos = s;
    //找到第一个有效位置pos
    while( *pos != 0 && ( *pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' ) )
    {
        pos++;
    }
    //若第一个有效位置是'\0'，说明整个字符串s都是无效的
    if( *pos == '\0' )
    {
        *s = '\0';
        return ;
    }
    
    while( *pos != '\0' )
    {
        *s++ = *pos++;
    }
    *s = '\0';
}
void Rtrim(char* s)
{
    int len = strlen(s);
    char* pos = &s[len - 1];
    //注意，除换行符很重要！！！因为没有除换行符导致bug：打开日志文件失败，因为载入内存的日志文件名后面有个隐藏的换行符'\r'!!!!
    while( pos != s && ( *pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r'  ) )
    {
        *pos-- = '\0';
    }

}

//输入字符串str,将其翻转
void inverse_string(char* str)
{
    int len = strlen(str);
    //是i<len/2，不能是i<=len/2
    for( int i=0; i<len/2; i++ )
    {
        char t = str[i];
        str[i] = str[len-i-1];
        str[len-i-1] = t;
    }
}

//给出int变量num，将其转换成字符串，存在以str为起始地址的串中
void int_to_string(int num,char* str,int max_len)
{
    char* iter = str;
    int t;
    int cur_len = 0;
    if( num == 0 )
    {
        *iter++ = '0';
        *iter++ = '\0';
        return ;
    }
    while( num && cur_len < max_len )
    {
        t = num % 10;
        num /= 10;
        *iter++ = t + '0';
        cur_len++;
    }
    *iter++ = '\0';
    inverse_string(str);
}



#endif