/*
 * @Author: lizhiyuan
 * @Date: 2020-12-01 14:20:04
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2020-12-01 14:26:35
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define SERVER_PORT 3000

// http服务器的版本号
