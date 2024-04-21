/*和 内存管理 有关的放在这个文件里 */

#include "ngx_global.h"
#include "ngx_c_memory.h"


CMemory* CMemory::m_instance = NULL;

//分配内存。memCount参数是要分配的内存字节数。
void* CMemory::AllocMemory(int memCount,bool ifmemset)
{
    void* tmpData = (void*)new char[memCount];
    //要求内存清零
    if( ifmemset )
    {
        memset(tmpData,0,memCount);
    }
    return tmpData;
}

//内存释放
void CMemory::FreeMemory(void* point)
{
    //这里不转回char*编译器会警告：warning: deleting void* is undefined.
    delete [] ( (char*)point );     //new的时候分配的是char[],所以转回char*
}
