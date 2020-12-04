<!--
 * @Author: lizhiyuan
 * @Date: 2019-10-06 15:06:49
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2020-12-04 14:16:55
-->
# libuv源码分析

## 1. socket阻塞模式和非阻塞模式

网络传输数据链路分析

```js


客户端A ------> write(进程被阻塞) -----> sys_write(系统调用,软中断) ------> 数据写入内核缓冲区(写缓冲区) -----> 网卡封包 ------->  路由器/交换机 ------->  服务器网卡发现数据到来产生硬中断(CPU进行系统调用,切换到内核态)-----> 内核放入该套接字的读缓冲区 ----> epoll_wait唤醒套接字进程


```

![](./image/socket.png)

- connect
- accept
- send / write
- recv / read
















