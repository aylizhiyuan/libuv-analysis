/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:35
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-21 14:36:48
 */
#include "fmacros.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "anet.h"

/**
 * @description: 打印当前的错误输出字符串
 * @param {*}
 * @return {*}
 */
static void anetSetError(char *err,const char *fmt,...){
    // va_list是一个字符串指针,可以理解为指向当前参数的一个指针,取参根据指针来
    va_list ap;
    if(!err) return;
    // ap初始化,指向可变参数的第一个参数
    va_start(ap,fmt);
    // 将错误字符串格式化输出
    vsnprintf(err,ANET_ERR_LEN,fmt,ap);
    // 将ap指针关闭
    va_end(ap);
}
// 将一个文件的句柄设置为阻塞模式,默认就是阻塞模式
int anetSetBlock(char *err,int fd,int non_block){
    int flags;
    // 获取文件状态标记
    if((flags = fcntl(fd,F_GETFL)) == -1){
        anetSetError(err, "fcntl(F_GETFL): %s", strerror(errno));
        return ANET_ERR;
    }
    if(non_block){
        // 按位进行或运算
        flags |= O_NONBLOCK; // 非阻塞
    }else{
        // 按位取反后与运算赋值
        flags &= ~O_NONBLOCK; // 非阻塞
    }
    // 设置文件的状态标记
    if(fcntl(fd,F_SETFL,flags) == -1){
        anetSetError(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetNonBlock(char *err,int fd){
    return anetSetBlock(err,fd,1); // 设置为非阻塞模式
}

int anetBlock(char *err,int fd){
    return anetSetBlock(err,fd,0); // 设置为阻塞模式
}

int anetKeepAlive(char *err,int fd,int interval){
    int val = 1; // 1为开启,0为缺省
    // 一般是为了检测TCP连接的情况,导致TCP断联(网线故障/断电)
    // 应用层可发送心跳包,自定义心跳包的消息头,一般由客户端主动发送
    // 此处是TCP协议自带的保活功能
    if(setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&val,sizeof(val)) == -1){
        anetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return ANET_ERR;
    }
    #ifdef __liunx__
        val = interval;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
            anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
            return ANET_ERR;
        }
        val = interval/3;
        if (val == 0) val = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
            anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
            return ANET_ERR;
        }
        val = 3;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
            anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
            return ANET_ERR;
        }
    #else
        ((void) interval);
    #endif
        return ANET_OK;
}

/**
 * @description: 设置TCP的延迟确认关闭还是开启
 * @param {*}
 * @return {*}
 */
static int anetSetTcpNoDelay(char *err,int fd,int val){
    if(setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&val,sizeof(val)) == -1){
        anetSetError(err,"setsockopt TCP_NODELAY:%s",strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

// 开启(默认开启)延迟确认
int anetEnableTcpNoDelay(char *err,int fd){
    return anetSetTcpNoDelay(err,fd,1);
}

// 关闭延迟确认,启用快速确认
int anetDisableTcpNoDelay(char *err,int fd){
    return anetSetTcpNoDelay(err,fd,0);
}

/**
 * @description:  设置发送缓冲区的大小,实际是8688个字节,如果数据较大,可重新设置
 * @param {char} *err
 * @param {int} fd
 * @param {int} buffsize
 * @return {*}
 */
int anetSetSendBuffer(char *err,int fd,int buffsize){
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, sizeof(buffsize)) == -1){
        anetSetError(err, "setsockopt SO_SNDBUF: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description:  设置keepalive开启
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
int anetTcpKeepAlive(char *err,int fd){
    int yes = 1;
    if(setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes)) == -1){
        anetSetError(err,"setsockopt SO_KEEPALIVE:%s",strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description:  设置socket超时时间
 * @param {char} *err
 * @param {int} fd
 * @param {longlong} ms
 * @return {*}
 */
int anetSendTimeout(char *err,int fd,long long ms){
    struct timeval tv;
    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000)*1000;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        anetSetError(err, "setsockopt SO_SNDTIMEO: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description: 解析主机名到ipbuf中去
 * @param {char} *err
 * @param {char} *host
 * @param {char} *ipbuf
 * @param {size_t} ipbuf_len
 * @param {int} flags 用来指定如何处理地址和名字
 * @return {*}
 */
int anetGenericResolve(char *err,char *host,char *ipbuf,size_t ipbuf_len,int flags){
    // 创建一个地址信息结构体
    struct addrinfo hints,*info;
    int rv;
    // 开辟内存空间
    memset(&hints,0,sizeof(hints));
    // 以端口号返回服务
    if(flags & ANET_IP_ONLY) hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;// 未指定
    hints.ai_socktype = SOCK_STREAM; // TCP
    if ((rv = getaddrinfo(host, NULL, &hints, &info)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    // 对获取的IP地址做判断
    if (info->ai_family == AF_INET) {
        // 获取 IP地址
        struct sockaddr_in *sa = (struct sockaddr_in *)info->ai_addr;
        inet_ntop(AF_INET, &(sa->sin_addr), ipbuf, ipbuf_len);
    } else {
        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)info->ai_addr;
        inet_ntop(AF_INET6, &(sa->sin6_addr), ipbuf, ipbuf_len);
    }

    freeaddrinfo(info);
    return ANET_OK;
}

int anetResolve(char *err,char *host,char *ipbuf,size_t ipbuf_len){
    return anetGenericResolve(err,host,ipbuf,ipbuf_len,ANET_NONE);
}

int anetResolveIP(char *err,char *host,char *ipbuf,size_t ipbuf_len){
    return anetGenericResolve(err,host,ipbuf,ipbuf_len,ANET_IP_ONLY);
}
// 设置服务器bind一个地址,即使这个地址当前已经存在已建立的连接
static int anetSetReuseAddr(char *err, int fd) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetCreateSocket(char *err,int domain){
    int s;
    // 创建一个套接字
    if((s = socket(domain,SOCK_STREAM,0)) == -1){
        anetSetError(err,"createing socket:%s",strerror(errno));
        return ANET_ERR;
    }
    if(anetSetReuseAddr(err,s) == ANET_ERR){
        close(s);
        return ANET_ERR;
    }
    return s;
}

#define ANET_CONNECT_NONE 0
#define ANET_CONNECT_NONBLOCK 1
#define ANET_CONNECT_BE_BINDING 2
// 创建一个TCP的连接
static int anetTcpGenericConnect(char *err,char *addr,int port,char *source_addr,int flags){
    int s = ANET_ERR,rv;
    char portstr[6];
    struct addrinfo hints,*servinfo,*bservinfo,*p,*b;
    
}

