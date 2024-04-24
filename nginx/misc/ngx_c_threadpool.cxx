#include "ngx_c_threadpool.h"
#include "ngx_c_memory.h"
#include "ngx_macro.h"
#include "ngx_c_socket.h"
#include "ngx_c_slogic.h"

extern CLogicSocket g_socket;       //socket全局对象
extern CThreadPool g_threadpool;   //线程池全局对象
//这两个宏都是系统定义的,用这个宏初始化了的互斥锁就不用再调用pthread_mutex_init()来初始化了
pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;    
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;

bool CThreadPool::m_shutdown = false;

//构造函数
CThreadPool::CThreadPool()
{
    m_iRunningThreadNum = 0;        //正在运行的线程数
    m_iLastEmgTime = 0;             //上次报告线程不够用了的时间
}

//析构函数
CThreadPool::~CThreadPool()
{

}

//线程池的初始化函数。在ngx_worker_process_init()中被调用
//返回值：所有线程都创建成功则返回true，出现错误则返回false。
bool CThreadPool::Create(int threadNum)
{
    ThreadItem* pNew;
    int err;

    m_iThreadNum = threadNum;

    for( int i=0; i<m_iThreadNum; i++ )
    {
        //this指针为这个线程池对象，ThreadItem的构造函数将这个线程池对象的指针赋值给new出来的ThreadItem->_pthis
        m_threadVector.push_back(pNew = new ThreadItem(this));
        //pthread_create是POSIX提供的创建线程的函数。
        //创建一个线程，将它的线程标识符存在pNew->_Handle中，新线程的属性为NULL，新线程将要执行的函数为ThreadFunc,传递给ThreadFunc的参数为pNew.
        //注意，新线程的执行顺序和主线程的执行顺序是不确定的，它们可能会交错执行。
        err = pthread_create(&pNew->_Handle,NULL,ThreadFunc,pNew);
        
        if( err != 0 )
        {
            //创建线程有错
            printf("CThreadPool::Create()中，创建线程%d失败!,错误码为%d\n",i,err);
            return false;
        }
        else 
        {
            //创建线程成功
        }

    }//end of for

    //下面的这部分代码保证所有线程都完全启动起来(运行到了pthread_cond_wait())，本函数才返回。
    std::vector<ThreadItem*>::iterator iter;
lblfor:
    for( iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter )
    {
        //这说明还有没完全启动的线程
        if( (*iter)->ifrunning == false )   
        {
            usleep(100*1000);       //睡眠100毫秒：usleep的单位是微秒。
            goto lblfor;
        }
    }
    return true;
}

//线程入口函数，该函数是一个CThreadPool的静态函数
void* CThreadPool::ThreadFunc(void* threadData)
{
    //printf("ThreadFunc开始执行了,也就是说，一个线程:tid=%lu创建成功了！\n",pthread_self());
    ThreadItem* pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool* pThreadPoolObj = pThread->_pThis;

    char* jobbuf = NULL;
    CMemory* p_memory = CMemory::getInstance();
    int err;

    pthread_t tid = pthread_self();         //获取线程自身的id
    //printf("线程tid=%lu的ThreadFunc初始化完毕了!\n",tid);

#if 0
    //只能通过条件变量的pthread_cond_signal()唤醒线程，若任务大量地涌入，任务会在消息队列中堆积，无法处理
    while( true )
    {
        err = pthread_mutex_lock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中,pthread_mutex_lock()失败了!\n");
        }
        else 
        {
            printf("线程tid=%lu获取到了锁m_pthreadMutex\n",tid);
        }
        pThread->ifrunning = true;
        
        //printf("目前消息队列中的消息数量是:%d\n",g_socket.getMsgRecvQueue().size());
        
        pthread_cond_wait(&m_pthreadCond,&m_pthreadMutex);
        err = pthread_mutex_unlock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中,pthread_mutex_unlock()失败了!\n");
        }
        else 
        {
            printf("线程tid=%lu释放了锁m_pthreadMutex\n",tid);
        }
        jobbuf=g_socket.outMsgRecvQueue();
        ++pThreadPoolObj->m_iRunningThreadNum;     
        printf("线程池中的一个线程开始执行任务了！tid=%lu\n",tid); 
        sleep(2);
        printf("线程池中的一个线程执行完任务了！tid=%lu\n",tid);
        p_memory->FreeMemory(jobbuf);               
        --pThreadPoolObj->m_iRunningThreadNum;      
    }
#endif

#if 1
    while( true )
    {
        err = pthread_mutex_lock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中pthread_mutex_lock()失败，返回的错误码为%d\n",err);
        }
        else 
        {
           // printf("线程tid=%lu拿到了锁m_pthreadMutex\n",tid);
        }
        //printf("目前消息队列中的消息数量是:%ld\n",g_socket.getMsgRecvQueue().size());
        //当StopAll被调用之后，被阻塞在pthread_cond_wait()的线程会停止阻塞继续往下走；而StopAll()中会将m_shutdown置为true，while循环结束。
        while( ( pThreadPoolObj->m_MsgRecvQueue.size() == 0 ) && m_shutdown == false )
        {
            pThread->ifrunning = true;
            //程序执行到这里，才认为一个线程已经完全启动成功
            pthread_cond_wait(&m_pthreadCond,&m_pthreadMutex);
        }

        //退出线程
        if( m_shutdown )
        {
            //pthread_mutex_unlock(&m_pthreadMutex);
            break;
        }

        printf("一个线程tid=%lu准备开始从消息队列中取消息了！此时该线程持有锁。\n",tid);
        //这里取出消息。注意：这里互斥还是锁着的状态
        char* jobbuf = pThreadPoolObj->m_MsgRecvQueue.front();
        pThreadPoolObj->m_MsgRecvQueue.pop_front();
        --pThreadPoolObj->m_iRecvMsgQueueCount;

        err = pthread_mutex_unlock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中pthread_cond_wait()失败，返回的错误码为%d\n",err);
        }
        else 
        {
            //printf("线程tid=%lu释放了锁m_pthreadMutex\n",tid);
        }
 
        ++pThreadPoolObj->m_iRunningThreadNum;      //原子+1

        //这里执行业务逻辑
        // printf("线程池中的一个线程开始执行任务了！tid=%lu\n",tid);   //这里用%d和%u打印都会出现错误
        // sleep(2);
        // printf("线程池中的一个线程执行完任务了！tid=%lu\n",tid);

        g_socket.threadRecvProcFunc(jobbuf);        //处理这条消息。
        
        p_memory->FreeMemory(jobbuf);               //任务处理完了，释放掉这段内存
        --pThreadPoolObj->m_iRunningThreadNum;      //原子-1
    }//end while(true)
#endif 

    return (void*)0;
}

void CThreadPool::StopAll()
{
    //保证StopAll函数在整个程序中只被调用一次
    if( m_shutdown )
    {
        return ;
    }
    m_shutdown = true;

    //激发所有处于wait状态的线程，使其停止阻塞，程序继续往下走。
    int err = pthread_cond_broadcast(&m_pthreadCond);
    if( err != 0 )
    {
        printf("CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d\n",err);
        return ;
    }
    
    //等待线程，让线程返回
    std::vector<ThreadItem*>::iterator iter;
    for( iter=m_threadVector.begin(); iter!=m_threadVector.end(); ++iter )
    {
        //int pthread_join(pthread_t thread, void ** retval); 
        //第一个参数指定接收哪个线程的返回值，第二个参数用一个二级指针接收该返回值。
        //pthread_join() 函数会一直阻塞调用它的线程，直至目标线程执行结束（接收到目标线程的返回值），阻塞状态才会解除。
        pthread_join((*iter)->_Handle,NULL);
    }
    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);
 
    for( iter=m_threadVector.begin(); iter!=m_threadVector.end(); ++iter )
    {
        if(*iter)
            delete *iter;
    }
    m_threadVector.clear();

    //printf("CThreadPool::StopAll()成功返回，线程池中线程全部正常结束!\n");
    return ;

}

void CThreadPool::inMsgRecvQueueAndSignal(char* buf)
{
    //互斥
    int err = pthread_mutex_lock(&m_pthreadMutex);
    if( err != 0 )
    {
        printf("CThreadPool::inMsgRecvQueueAndSignal()中，pthread_mutex_lock()失败!,错误码为%d\n");
    }
    m_MsgRecvQueue.push_back(buf);
    printf("将消息%s加入消息队列成功，此时消息队列中有%lu条消息。\n",buf,m_MsgRecvQueue.size());
    ++m_iRecvMsgQueueCount;

    //取消互斥
    err = pthread_mutex_unlock(&m_pthreadMutex);
    if( err != 0 )
    {
        printf("CThreadPool::inMsgRecvQueueAndSignal()中，pthread_mutex_unlock()失败,错误码为%d\n",err);
    }

    Call();

}

//一个包收完整后，ngx_wait_request_handler_proc_plast()函数将包扔进消息队列，并调用Call来激发一个线程处理这个消息任务。
void CThreadPool::Call()
{
    int err = pthread_cond_signal(&m_pthreadCond);
    if( err != 0 )
    {
        printf("CThreadPool::Call()中pthread_cond_signal()失败，返回的错误码为%d\n",err);
    }

    //线程池中没有空闲线程了
    if( m_iThreadNum == m_iRunningThreadNum )
    {
        printf("CThreadPool::Call()中发现空闲线程数量为0，考虑一下扩容线程？\n");
    }



}
