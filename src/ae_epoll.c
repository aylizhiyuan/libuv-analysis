/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:10
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-11 15:55:50
 */
#include <sys/epoll.h>

// 创建一个结构体
typedef struct aeApiState {
    int epfd; // 由epoll_create生成的epoll专用的文件描述符
    struct epoll_event *events; // 要监听的事件类型(EPOLLIN)和socket句柄信息(listenfd)
} aeApiState

// 应该是创建一个loop
static int aeApiCreate(aeEventLoop *eventLoop){
    aeApiState *state = zmalloc(sizeof(aeApiState));
    if(!state) return -1;
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if(!state->events){
        zfree(state);
        return -1;
    }
    // 创建一个epoll
    state->epfd = epoll_create(1024);
    if(state->epfd == -1){
        zfree(state->events);
        zfree(state);
        return -1;
    }
    // 传递的eventLoop->apidata = state(事件循环的具体参数)
    eventLoop->apidata = state;
    return 0;
}

static int aeAPiResize(aeEventLoop *eventLoop,int setsize){
    aeApiState *state = eventLoop->apidata;
    state->events = zrealloc(state->events, sizeof(struct epoll_event)*setsize);
    return 0;   
}
static void aeApiFree(aeEventLoop *eventLoop){
    
}
static int aeAPiAddEvent(aeEventLoop *eventLoop,int fd,int mask){

}
static void aeApiDelEvent(aeEventLoop *eventLoop,int fd,int delmask){

}
static int aeApiPoll(aeEventLoop *eventLoop,struct timeval *tvp){

}
// 返回使用的API名称
static char *aeAPiName(void){
    return "epoll";
}