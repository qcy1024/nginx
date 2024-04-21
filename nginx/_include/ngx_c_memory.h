#ifndef __NGX_C_MEMORY__H
#define __NGX_C_MEMORY__H

#include "ngx_global.h"

//内存管理相关的类，单例类
class CMemory
{
private:
    CMemory() {};       //单例类的构造函数是私有的
public:
    ~CMemory() {};

private:
    static CMemory* m_instance;

public:
    static CMemory* getInstance()
    {
        //加锁
        if( m_instance == NULL )
        {
            if( m_instance == NULL )
            {
                m_instance = new CMemory();
                
            }
            //放锁
        }
        return m_instance;
    }
    

public:
    void* AllocMemory(int memCount,bool ifmemset);
    void FreeMemory(void* point);

};

 

#endif
