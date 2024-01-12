

#include "ngx_c_conf.h"
#include <cstdio>

int main()
{
    //获取一个单例类对象指针，用于读取并解析配置文件
    CConfig *p_config = CConfig::GetInstance();
    
    //CConfig的单例类对象p_config实际上就通过其成员m_ConfigItrmList保存的所有的配置文件条目。
    if( p_config->Load("nginx.conf") == false )
    {
        printf("配置文件载入失败，退出！\n");
        return 1;
    }
    int i = 0;
    for( std::vector<LPCConfItem>::iterator it = p_config->m_ConfigItemList.begin(); it != p_config->m_ConfigItemList.end(); ++it )
    {

        printf("条目%d,名字是:%s, 内容是:%s\n",i++,(*it)->ItemName,(*it)->ItemContent );
    }


    return 0;
}

