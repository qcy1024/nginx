#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

/*这个文件是读取配置文件的单例类的.h文件。有一个vector的数据成
员用来存储配置文件信息的列表，还有三个函数成员。*/
#include "ngx_global.h"
#include <vector>
#include <cstddef>

class CConfig
{
private:
    CConfig();
public:
    ~CConfig();
private:
    static CConfig *m_instance;

public:
    static CConfig* GetInstance()
    {
        if( m_instance == NULL )
        {
            m_instance = new CConfig;
            static CGarhuishou c1;
        }
        return m_instance;
    }

    class CGarhuishou
    {
    public:
        ~CGarhuishou()
        {
            if( CConfig::m_instance )
            {
                delete CConfig::m_instance;
                CConfig::m_instance = NULL;
            }    
        }
    };

//以上代码将CConfig实现为一个单例类，以下代码实现配置文件的读取和解析
public:
    bool Load(const char* pconfName);   //装载配置文件
    char* GetString(const char* p_itemname);
    int GetIntDefault(const char* p_itemname,const int def);

public:
    //LPCConfItem是CConfItem的指针类型，CConfItem中有两个字符串变量，分别表示条目名字和条目内容
    std::vector<LPCConfItem> m_ConfigItemList;  //存储配置信息的列表


};




#endif