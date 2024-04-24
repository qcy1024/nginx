#ifndef __NGX_C_CRC32_H__
#define __NGX_C_CRC32_H__

#include "ngx_global.h"

//
class CCRC32 
{
private:
    CCRC32();
public:
    ~CCRC32();
private:
    static CCRC32* m_instance;

public:
    static CCRC32* getInstance()
    {
        if( m_instance == NULL )
        {
            //锁
            if( m_instance == NULL )
            {
                m_instance = new CCRC32();
            }
            //解锁
        }
        return m_instance;
    }

public:
    void initCRC32Table();

    unsigned int Reflect(unsigned int ref, char ch);

    int getCRC(unsigned char* buffer, unsigned int dwSize);

public:
    unsigned int crc32_table[256];

};

#endif 