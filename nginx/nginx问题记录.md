2024.01



01.16：

在编写ngx_log_init()函数时，遇到了一个bug：日志文件无法打开，并且在以下if中：

```c
if( ngx_log.log_fd < 0 )
{
    ngx_log_stderr(0,"日志文件%s载入失败，退出！\n",log_file_name);
    ngx_log.log_fd = STDERR_FILENO;
}
```

ngx_log_stderr()打印的格式串参数%s,当以字符串常量"error.log"传入时，可以正常打印信息到屏幕上，但是当以char* 类型的变量log_file_name为参数传入时，打印的信息就会很奇怪：

![image-20240116162113186](C:\Users\29246\AppData\Roaming\Typora\typora-user-images\image-20240116162113186.png)

一开始怀疑是给%s的参数为字面值和char*变量的区别，解决了很久，没有结果。后来开始怀疑是不是log_file_name的结尾没有写入'\0'，于是开始排查，倒是没排查出来它结尾没有'\0'，反倒是发现配置项中的每一项，CConfItem.ItemName的strlen()都比配置文件中的要长1，于是想到了末尾没有除换行符'\r'这件事，在Rtrim()以及Ltrim()函数中加入相关代码，问题解决。配置文件可以正常打开，并且if中的打印信息也能正常显示了。









