/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:10
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-11 17:26:03
 */
#include <sys/epoll.h>
typedef struct aeApiState {
    int epfd; // epoll_create
    struct epoll_event *events; // 接收发生事件的数组,分为events和data两个部分
} aeApiState;

// 给eventLoop中创建一个新的epoll实例,放到apidata中
// epoll_ctl(epoll_create实例,动作,socket,监听的epll_event数组)
// epoll_wait(epoll_create实例,接收结果的epoll_event数组)
static int aeApiCreate(aeEventLoop *eventLoop){
    aeApiState *state = zmalloc(aeApiState)
    if(!state) return -1;
    // 需要监听的事件数组开辟空间
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize)
    if(!state->events){
        zfree(state);
        return -1;
    }
    // 创建epoll实例
    state->epfd = epoll_create(1024);
    if(state->epfd == -1){
        zfree(state->events);
        zfree(state);
        return -1;
    }
    // 初始化Loop中的apidata
    eventLoop->apidata = state;
    return 0;
}
// 改变事件数组的大小
static int aeApiResize(aeEventLoop *eventLoop,int setsize){
    aeApiState *state = eventLoop->apidata; // 先获取loop中的实例
    state->events = zrealloc(state->events,sizeof(struct epoll_event)*setsize);
    return 0;
}
// 释放空间
static void aeApiFree(aeEventLoop *eventLoop){
    aeApiState *state = eventLoop->apidata;
    close(state->epfd);
    zfree(state->events);
    zfree(state);
}
// 添加事件给epoll_event数组
static int aeApiAddEvent(aeEventLoop *eventLoop,int fd,int mask){
    aeApiState *state = eventLoop->apidata; //先拿到epoll实例
    struct epoll_event ee = {0};
    // 判断如果当前添加的fd已经在数组中了，就改为修改..否则为新增
    int op = eventLoop->events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0; //初始化事件类型
    mask |= eventLoop->events[fd].mask; // EPOLL_IN or EPOLL_OUT
    if(mask & AE_READABLE) ee.events |= EPOLLIN;
    if(mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if(epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;
    return 0;
}
// 从event_event数组中将事件删除掉
static void aeApiDelEvent(aeEventLoop *eventLoop,int fd,int delmask){
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0};
    int mask = eventLoop->event[fd].mask & (~delmask);
    ee.events = 0;
    if(mask & AE_READABLE) ee.events |= EPOLLIN;
    if(mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if(mask != AE_NONE){
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    }else{
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}
static int aeApiPoll(aeEventLoop *eventLoop,struct timeval *tvp){
    aeAPiState *state = eventLoop->apidata;
    int retval,numevents = 0;
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if(retval > 0){
        int j;
        numevents = retval;
        for(j=0;i<numevents;j++){
            // 循环我们发生事件的列表
            int mask = 0;
            struct epoll_event *e = state->events + j; // 取出发生事件的fd
            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE;
            // 将发生的事件fd存入eventLoop
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
static char *aeApiName(void){
    return "epoll";
}

