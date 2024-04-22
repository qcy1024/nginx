#ifndef __NGX_LOCKMUTEX_H__
#define __NGX_LOCKMUTEX_H__

#include <pthread.h>

class CLock
{
public:
    CLock(pthread_mutex_t* pMutex)
    {
        m_pMutex = pMutex;
        //pthread_mutex_lock()是POSIX标准提供的函数，用于获取一个互斥(对互斥加锁)。
        pthread_mutex_lock(m_pMutex);
    };
    ~CLock()
    {
        pthread_mutex_unlock(m_pMutex);
    }


private:
    pthread_mutex_t* m_pMutex;
};

#endif

