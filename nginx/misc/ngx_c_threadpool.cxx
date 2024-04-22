#include "ngx_c_threadpool.h"
#include "ngx_c_memory.h"
#include "ngx_macro.h"
#include "ngx_c_socket.h"
extern CSocket g_socket;       //socket全局对象

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
    //m_iPrintInfoTime = 0;         //上次打印参考信息的时间
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
    ThreadItem* pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool* pThreadPoolObj = pThread->_pThis;

    char* jobbuf = NULL;
    CMemory* p_memory = CMemory::getInstance();
    int err;

    pthread_t tid = pthread_self();         //获取线程自身的id
    while(true)
    {
        err = pthread_mutex_lock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中pthread_mutex_lock()失败，返回的错误码为%d\n",err);
        }

        //当StopAll被调用之后，被阻塞在pthread_cond_wait()的线程会停止阻塞继续往下走；而StopAll()中会将m_shutdown置为true，while循环结束。
        while( (jobbuf=g_socket.outMsgRecvQueue()) == NULL && m_shutdown == false )
        {
            if( pThread->ifrunning == false )
            {
                pThread->ifrunning = true;
            }
            //程序执行到这里，才认为一个线程已经完全启动成功
            pthread_cond_wait(&m_pthreadCond,&m_pthreadMutex);
        }


        err = pthread_mutex_unlock(&m_pthreadMutex);
        if( err != 0 )
        {
            printf("CThreadPool::ThreadFunc()中pthread_cond_wait()失败，返回的错误码为%d\n",err);
        }

        //退出线程
        if( m_shutdown )
        {
            if( jobbuf != NULL )
            {
                p_memory->FreeMemory(jobbuf);
            }
            break;
        }

        ++pThreadPoolObj->m_iRunningThreadNum;      //原子+1

        //这里执行业务逻辑
        printf("线程池中的一个线程开始执行任务了！tid=%lu\n",pthread_self());   //这里用%d和%u打印都会出现错误
        sleep(2);
        printf("线程池中的一个线程执行完任务了！tid=%lu\n",pthread_self());
        
        p_memory->FreeMemory(jobbuf);               //任务处理完了，释放掉这段内存
        --pThreadPoolObj->m_iRunningThreadNum;      //原子-1
    }//end while(true)


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

void CThreadPool::Call(int irmqc)
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
