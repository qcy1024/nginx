#ifndef __NGINX_STRING__CXX
#define __NGINX_STRING__CXX

/*一些和字符串处理相关的函数放在这个文件里面*/
#include <string.h>


//输入字符串source，除掉其左侧无效字符(空格、回车、制表符)
void Ltrim(char* s)
{
    char* pos = s;
    //找到第一个有效位置pos
    while( *pos != 0 && ( *pos == ' ' || *pos == '\t' || *pos == '\n' ) )
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
    while( pos != s && ( *pos == ' ' || *pos == '\t' || *pos == '\n' ) )
    {
        *pos-- = '\0';
    }

}

#endif