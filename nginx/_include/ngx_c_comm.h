#ifndef __NGX_C_COMM_H__
#define __NGX_C_COMM_H__


#define _PKG_MAX_LENGTH 30000   //每个包的最大长度【包头+包体】

//通信 收包状态定义
#define _PKG_HD_INIT    0   //初始状态，准备接受数据包头
#define _PKG_HD_RECVING 1   //接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT    2   //包头刚好收完，准备接收包体
#define _PKG_BD_RECVING 3   //接收包体中，包体不完整，继续接收中，处理后直接回到_PKG_HD_INIT状态。

#define _DATA_BUFSIZE_  20  //接收包头的数组的大小（20字节）

//为了防止出现字节对齐问题，所有在网络上传输的这种结构，必须都采用1字节对齐方式。
#pragma pack(1)     //按1字节对齐(结构之间的成员紧密地排列在一起)

//数据报的包头结构
typedef struct _COMM_PKG_HEADER
{
    unsigned short pkgLen;      //报文总长度【包头+包体】

    unsigned short msgCode;     //消息类型码
    int crc32;                  //CRC32校验，4字节
}COMM_PKG_HEADER, *LPCOMM_PKG_HEADER;

#pragma pack()      //pragma pack() 是一种编译器指令，用于控制结构体成员的内存对齐方式

#endif


