#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "ngx_global.h"
#include "ngx_c_conf.h"


//定义并初始化CConfig的静态成员
CConfig *CConfig::m_instance = NULL;

//构造函数
CConfig::CConfig()
{
    
}

//析构函数
CConfig::~CConfig()
{
    std::vector<LPCConfItem>::iterator pos;
    for( pos = m_configItemList.begin(); pos!= m_configItemList.end(); ++pos  )
    {
        delete (*pos);
    }
    m_configItemList.clear();
}

//装载配置文件
//这个函数实际上就是把配置文件nginx.conf中的所有带等号=的条目放到了CConfig的单例
//对象中的成员列表m_ConfigItemList中。
//***我们假设配置文件符合语法格式，只有多余的空格、制表符、回车换行。
bool CConfig::load(const char* pconfName)
{
    //FILE是C标准<stdio.h>定义的一种数据类型，包含了有关文件的信息。
    FILE* fp;
    //fopen()返回一个FILE* 类型的指针。
    fp = fopen(pconfName,"r");
    if( fp == NULL )
        return false;
    char linebuf[501];
    while( !feof(fp) )
    {
        //fgets()如果成功，该函数返回读到的字符串。如果到达文件末尾或者没有读取到任何字符，str 的内容保持不变，并返回一个空指针。
        //如果发生错误，返回一个空指针
        if( fgets(linebuf,500,fp) == NULL )
        {
            continue;
        }
        Ltrim(linebuf);
        Rtrim(linebuf);
        if( *linebuf == '\0' ) continue;
        if( linebuf[0] == ';' || linebuf[0] == '#' || linebuf[0] == '[' ) continue;
        //char *strchr(const char *str, int c)  返回字符串str中第一次出现字符c的位置的指针
        char* equal_sign_pos = strchr(linebuf,'=');
        if( equal_sign_pos != NULL )
        {
            //char *strncpy(char *dest, const char *src, size_t n) 把 src 所指向的字符串
            //复制到 dest，最多复制 n 个字符。当 src 的长度小于 n 时，dest 的剩余部分将用空字节填充
            CConfItem* tmpConfItem = new CConfItem;
            memset(tmpConfItem,0,sizeof(tmpConfItem));
            strncpy(tmpConfItem->ItemName,linebuf,equal_sign_pos-linebuf);
            strcpy(tmpConfItem->ItemContent,equal_sign_pos+1);

            Rtrim(tmpConfItem->ItemName);
            Ltrim(tmpConfItem->ItemContent);
            m_configItemList.push_back(tmpConfItem);
        }
        
    }
    //end while( !feof(fp) )
    fclose(fp);
    return 1;
}

//如果没找到这个配置项，则返回NULL
char* CConfig::getString(const char* p_itemname)
{
    std::vector<LPCConfItem>::iterator pos;
    for( pos = m_configItemList.begin(); pos != m_configItemList.end(); ++pos )
    {
        if( strcmp( (*pos)->ItemName,p_itemname ) == 0 )
        {
            return (*pos)->ItemContent;
        }
    }
    return NULL;
}

int CConfig::getIntDefault(const char* p_itemname,const int def)
{
    std::vector<LPCConfItem>::iterator pos;
    for( pos = m_configItemList.begin(); pos != m_configItemList.end(); ++pos )
    {
        if( strcmp( (*pos)->ItemName,p_itemname ) == 0 )
        {
            return atoi( (*pos)->ItemContent );
        }
    }
    //没找到返回默认值
    return def;
}