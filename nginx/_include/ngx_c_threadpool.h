#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include "ngx_global.h"


//线程池相关类
class CThreadPool
{
public:
    CThreadPool();
    ~CThreadPool();
public:
    bool Create(int threadNum);         //创建线程池中的所有线程
    void StopAll();                     //使线程池中的所有线程退出

    void inMsgRecvQueueAndSignal(char* buf);    //收到一个完整的数据包之后，入消息队列，并触发线程池中的线程来处理该任务。
    void Call();               //来任务了，调一个线程池中的线程下来干活

private:
    //POSIX标准规定，作为pthread_create()参数的线程入口函数，必须是返回类型为void*,并且参数列表仅有一个void*的函数。
    static void* ThreadFunc(void* threadData);  //新线程的线程入口函数

private:
    //一个线程条目
    struct ThreadItem 
    {
        pthread_t _Handle;          //线程句柄，即线程标识符
        CThreadPool* _pThis;        //记录该线程所属的线程池的指针
        bool ifrunning;             //标记是否正式启动起来

        ThreadItem(CThreadPool* pthis):_pThis(pthis),ifrunning(false) {};
        ~ThreadItem() {};
    };

private:
    static pthread_mutex_t              m_pthreadMutex;             //互斥量
    static pthread_cond_t               m_pthreadCond;              //线程同步条件变量
    static bool                         m_shutdown;                  //退出标志

    int                                 m_iThreadNum;               //要创建的线程数量
    std::atomic<int>                    m_iRunningThreadNum;        //运行中的线程数
    time_t                              m_iLastEmgTime;             //上次发生线程不够用【紧急事件】的时间

    std::vector<ThreadItem*>            m_threadVector;             //线程的容器，容器里就是各个线程

    //消息队列
    std::list<char*>                 m_MsgRecvQueue;        //接收数据消息队列
    int                              m_iRecvMsgQueueCount;  //消息队列目前的消息数量

};


#endif

