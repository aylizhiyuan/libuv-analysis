/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:35
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-10 11:30:44
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
/**
 * @description: 
 * @param {char} *err
 * @param {char} *addr
 * @param {int} port
 * @param {char} *source_addr
 * @param {int} flags
 * @return {*}
 */
static int anetTcpGenericConnect(char *err, char *addr, int port,
                                 char *source_addr, int flags)
{
    int s = ANET_ERR, rv;
    char portstr[6];  /* strlen("65535") + 1; */
    struct addrinfo hints, *servinfo, *bservinfo, *p, *b;

    snprintf(portstr,sizeof(portstr),"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(addr,portstr,&hints,&servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        /* Try to create the socket and to connect it.
         * If we fail in the socket() call, or on connect(), we retry with
         * the next entry in servinfo. */
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;
        if (anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        if (flags & ANET_CONNECT_NONBLOCK && anetNonBlock(err,s) != ANET_OK)
            goto error;
        if (source_addr) {
            int bound = 0;
            /* Using getaddrinfo saves us from self-determining IPv4 vs IPv6 */
            if ((rv = getaddrinfo(source_addr, NULL, &hints, &bservinfo)) != 0)
            {
                anetSetError(err, "%s", gai_strerror(rv));
                goto error;
            }
            for (b = bservinfo; b != NULL; b = b->ai_next) {
                if (bind(s,b->ai_addr,b->ai_addrlen) != -1) {
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

/* Like read(2) but make sure 'count' is read before to return
 * (unless error or EOF condition is encountered) */
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

/* Like write(2) but make sure 'count' is written before to return
 * (unless error is encountered) */
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

/* Format an IP,port pair into something easy to parse. If IP is IPv6
 * (matches for ":"), the ip is surrounded by []. IP and port are just
 * separated by colons. This the standard to display addresses within Redis. */
int anetFormatAddr(char *buf, size_t buf_len, char *ip, int port) {
    return snprintf(buf,buf_len, strchr(ip,':') ?
           "[%s]:%d" : "%s:%d", ip, port);
}

/* Like anetFormatAddr() but extract ip and port from the socket's peer. */
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

