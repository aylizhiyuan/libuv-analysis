/*
 * @Author: your name
 * @Date: 2021-02-21 13:46:25
 * @LastEditTime: 2021-03-17 10:53:48
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /libuv-analysis/src/anet.c
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
 * @description: 输出错误信息的函数
 * @param {char} *err 错误信息字符串
 * @param {constchar} *fmt 格式化参数
 * @return {*}
 */
static void anetSetError(char *err,const char *fmt,...){
    // va_list是解决可变参数的
    va_list ap; // 创建一个va_list变量,这个变量是指向参数的指针
    if(!err) return;
    // ap指向可变参数的第一个参数
    va_start(ap,fmt);
    vsnprintf(err,ANET_ERR_LEN,fmt,ap);
    // 清空
    va_end(ap);
}

/**
 * @description: 文件句柄设置为阻塞或者非阻塞
 * @param {char} *err
 * @param {int} fd
 * @param {int} non_block 1不阻塞或者是0阻塞
 * @return {*}
 */
int anetSetBlock(char *err,int fd,int non_block){
    int flags;
    // 获取文件open时候的状态
    if((flags = fcntl(fd,F_GETFL)) == -1){
        // strerror输出标准的错误输出....
        anetSetError(err,"fcntl(F_GETFL): %s",strerror(errno));
        return ANET_ERR; // 一般系统错误的返回code都是-1
    }
    if(non_block){
        flags |= O_NONBLOCK; //非阻塞
    }else{
        flags &= ~O_NONBLOCK; // 阻塞
    }
    // 设置文件的阻塞/非阻塞操作
    if(fcntl(fd,F_SETFL,flags) == -1){
        anetSetError(err,"fcntl(F_SETFL,O_NONBLOCK): %s",strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description:  设置为非阻塞模式
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
int anetNonBlock(char *err,int fd){
    return anetSetBlock(err,fd,1);
}
/**
 * @description: 设置为阻塞模式,默认
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
int anetBlock(char *err,int fd){
    return anetSetBlock(err,fd,0);
}

/**
 * @description:  设置TCP中是否开启keepalive功能
 * @param {char} *err
 * @param {int} fd
 * @param {int} interval
 * @return {*}
 */

// TCP默认是关闭keepalive功能的,主要是为了保活
int anetKeepAlive(char *err,int fd,int interval){
    int val = 1;
    if(setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&val,sizeof(val)) == -1){
        anetSetError(err,"setsockopt SO_KEEPALIVE: %s",strerror(errno));
        return ANET_ERR;
    }
#ifdef __linux__
    /* Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux. Modify settings to make the feature
     * actually useful. */

    /* Send first probe after interval. */
    val = interval;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
        anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
        return ANET_ERR;
    }

    /* Send next probes after the specified interval. Note that we set the
     * delay as interval / 3, as we send three probes before detecting
     * an error (see the next setsockopt call). */
    val = interval/3;
    if (val == 0) val = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
        anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
        return ANET_ERR;
    }

    /* Consider the socket in error state after three we send three ACK
     * probes without getting a reply. */
    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
        anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
        return ANET_ERR;
    }
#else
    ((void) interval); /* Avoid unused var warning for non Linux systems. */
#endif
    return ANET_OK;
}
/**
 * @description: 设置是否开启延迟确定,默认是开启的状态
 * @param {char} *err
 * @param {int} fd
 * @param {int} val
 * @return {*}
 */
static int anetSetTcpNoDelay(char *err,int fd,int val){
    if(setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&val,sizeof(val)) == -1){
        anetSetError(err,"setsockopt TCP_NODELAY: %s",strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description:  开启nodelay功能
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
int anetEnableTcpNoDelay(char *err,int fd){
    return anetSetTcpNoDelay(err,fd,1);
}

/**
 * @description: 关闭nodelay功能
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
int anetDisableTcpNoDelay(char *err,int fd){
    return anetSetTcpNoDelay(err,fd,0);
}
/**
 * @description: 设置发送缓冲区的大小
 * @param {char} *err
 * @param {int} fd
 * @param {int} buffsize
 * @return {*}
 */
int anetSetSendBuffer(char *err,int fd,int buffsize){
    if(setsockopt(fd,SOL_SOCKET,SO_SNDBUF,&buffsize,sizeof(buffsize)) == -1){
        anetSetError(err,"setsockopt SO_SNDBUF: %s",strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}
int anetTcpKeepAlive(char *err,int fd){
    int yes = 1;
    if(setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes)) == -1){
        anetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * @description:  设置超时时间
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
 * @description: 通过主机名称返回IP地址
 * @param {char} *err
 * @param {char} *host
 * @param {char} *ipbuf
 * @param {size_t} ipbuf_len
 * @param {int} flags
 * @return {*}
 */
int anetGenericResolve(char *err,char *host,char *ipbuf,size_t ipbuf_len,int flags){
    // 一个地址结构体,用来存放地址信息的
    struct addrinfo hints,*info;
    int rv;
    memset(&hints,0,sizeof(hints));
    // 以数字格式返回主机地址
    if(flags & ANET_IP_ONLY) hints.ai_flags = AI_NUMERICHOST;
    // 未指定协议族
    hints.ai_family = AF_UNSPEC;
    // socket类型,是普通的socket流协议
    hints.ai_socktype = SOCK_STREAM;
    // getaddinfo(域名,服务名,期望返回的信息类型,返回结果)
    if((rv = getaddrinfo(host,NULL,&hints,&info)) != 0){
        anetSetError(err,"%s",gai_strerror(rv));
        return ANET_ERR;
    }
    // 如果返回的是IPv4的协议
    if(info->ai_family == AF_INET){
        // info是指向addinfo结构体链表的指针
        struct sockaddr_in *sa = (struct sockaddr_in *)info->ai_addr;
        // 转化成整数的意思
        inet_ntop(AF_INET, &(sa->sin_addr), ipbuf, ipbuf_len);
    }else{
        // IPv6的协议
        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)info->ai_addr;
        inet_ntop(AF_INET6, &(sa->sin6_addr), ipbuf, ipbuf_len);
    }
    // 释放结果结构体
    freeaddrinfo(info);
    return ANET_OK;  
}

int anetResolve(char *err,char *host,char *ipbuf,size_t ipbuf_len){
    return anetGenericResolve(err,host,ipbuf,ipbuf_len,ANET_NONE);
}
int anetResolveIP(char *err,char *host,char *ipbuf,size_t ipbuf_len){
    return anetGenericResolve(err,host,ipbuf,ipbuf_len,ANET_IP_ONLY);
}
/**
 * @description: 默认端口在释放等待两分钟以后才能被再次使用,这里设置立即再次使用
 * @param {char} *err
 * @param {int} fd
 * @return {*}
 */
static int anetSetReuseAddr(char *err,int fd){
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}
/**
 * @description: 创建一个新的socket结构
 * @param {char} *err
 * @param {int} domain
 * @return {*}
 */
static int anetCreateSocket(char *err,int domain){
    int s;
    if((s = socket(domain,SOCK_STREAM,0)) == -1){
        anetSetError(err,"create socket %s",strerror(errno));
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
/**
 * @description: 客户端建立连接的过程
 * @param {char} *err
 * @param {char} *addr
 * @param {int} port
 * @param {char} *source_addr
 * @param {int} flags
 * @return {*}
 */
static int anetTcpGenericConnect(char *err,char *addr,int port,char *source_addr,int flags){
    int s = ANET_ERR,rv;
    char portstr[6];
    struct addrinfo hints,*servinfo,*bservinfo,*p,*b;
    snprintf(portstr,sizeof(portstr),"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    // 根据域名,拿到IP地址信息
    if((rv = getaddrinfo(addr,portstr,&hints,&servinfo)) != 0){
        anetSetError(err,"%s",gai_strerror(rv));
        return ANET_ERR;
    }
    // 拿到地址信息,可能一个域名有多个IP地址
    for(p = servinfo;p != NULL;p = p->ai_next){
        // 创建socket对象
        if((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        if(anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        if(flags & ANET_CONNECT_NONBLOCK && anetNonBlock(err,s) != ANET_OK){
            goto error;
        }
        // 这里我不知道为什么还要去bind一下这个地址信息
        if(source_addr){
            int bound = 0;
            if((rv = getaddrinfo(source_addr,NULL,&hints,&bservinfo)) != 0){
                anetSetError(err,"%s",gai_strerror(rv));
                goto error;
            }
            for(b = bservinfo;b != NULL;b->ai_next){
                if(bind(s,b->ai_addr,b->ai_addrlen) != -1){
                    bound = 1;
                    break;
                }
            }
            freeaddrinfo(bservinfo);
            if (!bound) {
                anetSetError(err, "bind: %s", strerror(errno));
                goto error;
            }
        }
        // 建立连接
        if (connect(s,p->ai_addr,p->ai_addrlen) == -1) {
            /* If the socket is non-blocking, it is ok for connect() to
             * return an EINPROGRESS error here. */
            if (errno == EINPROGRESS && flags & ANET_CONNECT_NONBLOCK)
                goto end;
            close(s);
            s = ANET_ERR;
            continue;
        }

        /* If we ended an iteration of the for loop without errors, we
         * have a connected socket. Let's return to the caller. */
        goto end;
    }
    if (p == NULL)
        anetSetError(err, "creating socket: %s", strerror(errno));

error:
    if (s != ANET_ERR) {
        close(s);
        s = ANET_ERR;
    }

end:
    freeaddrinfo(servinfo);

    /* Handle best effort binding: if a binding address was used, but it is
     * not possible to create a socket, try again without a binding address. */
    if (s == ANET_ERR && source_addr && (flags & ANET_CONNECT_BE_BINDING)) {
        return anetTcpGenericConnect(err,addr,port,NULL,flags);
    } else {
        return s;
    }
}
int anetTcpConnect(char *err, char *addr, int port)
{
    return anetTcpGenericConnect(err,addr,port,NULL,ANET_CONNECT_NONE);
}

int anetTcpNonBlockConnect(char *err, char *addr, int port)
{
    return anetTcpGenericConnect(err,addr,port,NULL,ANET_CONNECT_NONBLOCK);
}

int anetTcpNonBlockBindConnect(char *err, char *addr, int port,
                               char *source_addr)
{
    return anetTcpGenericConnect(err,addr,port,source_addr,
            ANET_CONNECT_NONBLOCK);
}

int anetTcpNonBlockBestEffortBindConnect(char *err, char *addr, int port,
                                         char *source_addr)
{
    return anetTcpGenericConnect(err,addr,port,source_addr,
            ANET_CONNECT_NONBLOCK|ANET_CONNECT_BE_BINDING);
}

int anetUnixGenericConnect(char *err, char *path, int flags)
{
    int s;
    struct sockaddr_un sa;

    if ((s = anetCreateSocket(err,AF_LOCAL)) == ANET_ERR)
        return ANET_ERR;

    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);
    if (flags & ANET_CONNECT_NONBLOCK) {
        if (anetNonBlock(err,s) != ANET_OK) {
            close(s);
            return ANET_ERR;
        }
    }
    if (connect(s,(struct sockaddr*)&sa,sizeof(sa)) == -1) {
        if (errno == EINPROGRESS &&
            flags & ANET_CONNECT_NONBLOCK)
            return s;

        anetSetError(err, "connect: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return s;
}

int anetUnixConnect(char *err, char *path)
{
    return anetUnixGenericConnect(err,path,ANET_CONNECT_NONE);
}

int anetUnixNonBlockConnect(char *err, char *path)
{
    return anetUnixGenericConnect(err,path,ANET_CONNECT_NONBLOCK);
}


/**
 * @description: 读操作
 * @param {int} fd
 * @param {char} *buf
 * @param {int} count
 * @return {*}
 */
int anetRead(int fd, char *buf, int count)
{
    ssize_t nread, totlen = 0;
    while(totlen != count) {
        nread = read(fd,buf,count-totlen);
        if (nread == 0) return totlen;
        if (nread == -1) return -1;
        totlen += nread;
        buf += nread;
    }
    return totlen;
}


/**
 * @description: 写操作
 * @param {int} fd
 * @param {char} *buf
 * @param {int} count
 * @return {*}
 */
int anetWrite(int fd, char *buf, int count)
{
    ssize_t nwritten, totlen = 0;
    while(totlen != count) {
        nwritten = write(fd,buf,count-totlen);
        if (nwritten == 0) return totlen;
        if (nwritten == -1) return -1;
        totlen += nwritten;
        buf += nwritten;
    }
    return totlen;
}

/**
 * @description: 监听 bind & listen
 * @param {char} *err
 * @param {int} s
 * @param {structsockaddr} *sa
 * @param {socklen_t} len
 * @param {int} backlog
 * @return {*}
 */
static int anetListen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog) {
    if (bind(s,sa,len) == -1) {
        anetSetError(err, "bind: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }

    if (listen(s, backlog) == -1) {
        anetSetError(err, "listen: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetV6Only(char *err, int s) {
    int yes = 1;
    if (setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,&yes,sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return ANET_OK;
}

static int _anetTcpServer(char *err, int port, char *bindaddr, int af, int backlog)
{
    int s = -1, rv;
    char _port[6];  /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port,6,"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(bindaddr,_port,&hints,&servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && anetV6Only(err,s) == ANET_ERR) goto error;
        if (anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        if (anetListen(err,s,p->ai_addr,p->ai_addrlen,backlog) == ANET_ERR) s = ANET_ERR;
        goto end;
    }
    if (p == NULL) {
        anetSetError(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1) close(s);
    s = ANET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}

int anetTcpServer(char *err, int port, char *bindaddr, int backlog)
{
    return _anetTcpServer(err, port, bindaddr, AF_INET, backlog);
}

int anetTcp6Server(char *err, int port, char *bindaddr, int backlog)
{
    return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog);
}

int anetUnixServer(char *err, char *path, mode_t perm, int backlog)
{
    int s;
    struct sockaddr_un sa;

    if ((s = anetCreateSocket(err,AF_LOCAL)) == ANET_ERR)
        return ANET_ERR;

    memset(&sa,0,sizeof(sa));
    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);
    if (anetListen(err,s,(struct sockaddr*)&sa,sizeof(sa),backlog) == ANET_ERR)
        return ANET_ERR;
    if (perm)
        chmod(sa.sun_path, perm);
    return s;
}

/**
 * @description: 三次握手后,如果有事件发生的时候accpet拿到发生事件的文件句柄
 * @param {char} *err
 * @param {int} s
 * @param {structsockaddr} *sa
 * @param {socklen_t} *len
 * @return {*}
 */
static int anetGenericAccept(char *err, int s, struct sockaddr *sa, socklen_t *len) {
    int fd;
    while(1) {
        fd = accept(s,sa,len);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                anetSetError(err, "accept: %s", strerror(errno));
                return ANET_ERR;
            }
        }
        break;
    }
    return fd;
}

int anetTcpAccept(char *err, int s, char *ip, size_t ip_len, int *port) {
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = anetGenericAccept(err,s,(struct sockaddr*)&sa,&salen)) == -1)
        return ANET_ERR;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin6_port);
    }
    return fd;
}

int anetUnixAccept(char *err, int s) {
    int fd;
    struct sockaddr_un sa;
    socklen_t salen = sizeof(sa);
    if ((fd = anetGenericAccept(err,s,(struct sockaddr*)&sa,&salen)) == -1)
        return ANET_ERR;

    return fd;
}

int anetPeerToString(int fd, char *ip, size_t ip_len, int *port) {
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (getpeername(fd,(struct sockaddr*)&sa,&salen) == -1) goto error;
    if (ip_len == 0) goto error;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else if (sa.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin6_port);
    } else if (sa.ss_family == AF_UNIX) {
        if (ip) strncpy(ip,"/unixsocket",ip_len);
        if (port) *port = 0;
    } else {
        goto error;
    }
    return 0;

error:
    if (ip) {
        if (ip_len >= 2) {
            ip[0] = '?';
            ip[1] = '\0';
        } else if (ip_len == 1) {
            ip[0] = '\0';
        }
    }
    if (port) *port = 0;
    return -1;
}


int anetFormatAddr(char *buf, size_t buf_len, char *ip, int port) {
    return snprintf(buf,buf_len, strchr(ip,':') ?
           "[%s]:%d" : "%s:%d", ip, port);
}


int anetFormatPeer(int fd, char *buf, size_t buf_len) {
    char ip[INET6_ADDRSTRLEN];
    int port;

    anetPeerToString(fd,ip,sizeof(ip),&port);
    return anetFormatAddr(buf, buf_len, ip, port);
}

int anetSockName(int fd, char *ip, size_t ip_len, int *port) {
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(fd,(struct sockaddr*)&sa,&salen) == -1) {
        if (port) *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin6_port);
    }
    return 0;
}

int anetFormatSock(int fd, char *fmt, size_t fmt_len) {
    char ip[INET6_ADDRSTRLEN];
    int port;

    anetSockName(fd,ip,sizeof(ip),&port);
    return anetFormatAddr(fmt, fmt_len, ip, port);
}








