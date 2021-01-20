/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:35
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-20 16:46:43
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


