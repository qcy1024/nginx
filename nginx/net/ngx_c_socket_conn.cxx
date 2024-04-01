//和 网络 中 连接 有关的函数放在这里

#include "ngx_c_socket.h"
#include "ngx_global.h"
#include "ngx_c_conf.h"


//从连接池中获取一个空闲连接
lpngx_connection_t CSocket::ngx_get_connection(int isock)
{
    lpngx_connection_t c = m_pfree_connections;         //空闲连接链表头
    
    if( c == NULL )
    {
        //系统应该能控制连接数量，防止空闲连接被耗尽，所以不应出现空闲链表为空的情况
        printf("CSocket::ngx_get_connection()中，空闲链表为空！\n");
        return NULL;
    }
    m_pfree_connections = c->data;                      //空闲链表头指针指向下一个未用的节点
    m_free_connection_n--;                              //空闲链表节点数量减少1

    //（1）先把c指向的对象中有用的东西搞出来保存成变量，因为这些数据可能会有用。
    uintptr_t instance = c->instance;
    uint64_t iCurrsequence = c->iCurrsequence;

    //（2）把以往有用的数据搞出来后，清空并给适当值
    memset(c,0,sizeof(ngx_connection_t));
    //isock是服务端正在监听的端口的套接字描述符，这里有80和443
    c->fd = isock;                                      

    //（3）这个值有用，所以在上边（1）中被保留，没有被清空，这里又把这个值赋回来
    c->instance = !instance;                            //官方nginx就这么写的，暂时不知道有啥用
    c->iCurrsequence = iCurrsequence;                   
    ++c->iCurrsequence;                                 //每次取用该值都应该增加1

    return c;

}

//归还参数c所代表的连接到连接池中，注意参数类型是lpngx_connection_t
void CSocket::ngx_free_connection(lpngx_connection_t c)
{
    c->data = m_pfree_connections;          //回收的节点指向原来串起来的空闲链的链头

    ++c->iCurrsequence;
    
    m_pfree_connections = c;
    ++m_free_connection_n;                  //空闲链表节点数量+1
    return ;

}

