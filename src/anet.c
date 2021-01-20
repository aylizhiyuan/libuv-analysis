/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:35
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-20 14:31:47
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


