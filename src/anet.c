/*
 * @Author: your name
 * @Date: 2021-02-21 13:46:25
 * @LastEditTime: 2021-03-15 11:30:06
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





