#include "ngx_global.h"
#include "ngx_c_crc32.h"

CCRC32* CCRC32::m_instance = NULL;

CCRC32::CCRC32()
{
    initCRC32Table();
}

CCRC32::~CCRC32()
{

}

//初始化crc32表辅助函数
unsigned int CCRC32::Reflect(unsigned int ref,char ch)
{
    unsigned int value(0);
    for( int i=1; i<(ch+1); i++ )
    {
        if( ref & 1 )
        {
            value |= 1 << (ch-i);
        }
    }
    return value;
}

void CCRC32::initCRC32Table()
{
    unsigned int ulPolynomial = 0x04c11db7;

    for( int i=0; i<=0xFF; i++ )
    {
        crc32_table[i] = Reflect(i,8) << 24;
        for( int j=0; j<8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 1 << 31 ) ? ulPolynomial : 0 );
        } 
        crc32_table[i] = Reflect(crc32_table[i],32);
    }
}

//接收一个内存其实地址，以及一个长度，计算出一个数字来，这个数字就叫做CRC32
int CCRC32::getCRC(unsigned char* buffer, unsigned int dwSize)
{
    unsigned int crc(0xffffffff);
    int len;
    len = dwSize;
    while( len-- )
    {
        crc = ( crc << 8 ) ^ crc32_table[ ( crc & 0xFF ) ^ *buffer++ ];
    }
    return crc ^ 0xffffffff;
}

