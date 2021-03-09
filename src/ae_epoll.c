/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:10
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-11 17:26:03
 */
#include <sys/epoll.h>
typedef struct aeApiState {
    int epfd; // epoll_create
    struct epoll_event *events; // 可以理解为需要监听的socket列表,分为events和data两个部分
} aeApiState;

// 给eventLoop中创建一个新的epoll实例,放到apidata中
// epoll_ctl(epoll_create实例,动作,socket,监听的epll_event数组)
// epoll_wait(epoll_create实例,接收结果的epoll_event数组)
static int aeApiCreate(aeEventLoop *eventLoop){
    

}
