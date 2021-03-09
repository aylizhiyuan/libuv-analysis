/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:10
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-11 17:26:03
 */
#include <sys/epoll.h>

// 创建一个结构体
typedef struct aeApiState {
    int epfd; // 由epoll_create生成的epoll专用的文件描述符
    struct epoll_event *events; // 哪些发生了读、写事件
} aeApiState

// add into EventLoop
static int aeApiCreate(aeEventLoop *eventLoop){
    // 创建一个结构体的指针,并分配内存
    aeApiState *state = zmalloc(sizeof(aeApiState));
    if(!state) return -1;
    // 创建我们的事件列表
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
    // 重新调整EventLoop的事件列表的长度
    state->events = zrealloc(state->events, sizeof(struct epoll_event)*setsize);
    return 0;   
}
// 删除我们的事件
static void aeApiFree(aeEventLoop *eventLoop){
    aeApiState *state = eventLoop->apidata;
    close(state->epfd);
    zfree(state->events);
    zfree(state);
}
// 给evetLoop.apiData添加事件
static int aeAPiAddEvent(aeEventLoop *eventLoop,int fd,int mask){
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0};
    int op = eventLoop->events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    // fd作为数组的键,将它加入event事件监听中去
    mask |= eventLoop->events[fd].mask;
    if(mask & AE_READABLE) ee.events |= EPOLLIN;
    if(mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if(epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;
    return 0;
}
static void aeApiDelEvent(aeEventLoop *eventLoop,int fd,int delmask){
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0};
    int mask = eventLoop->events[fd].mask & (~delmask);
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
    aeApiState *state = eventLoop->apidata;
    int retval,numevents = 0;
    // 返回发生事件的句柄数组
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1 )
    if(retval > 0){
        int j;
        numevents = retval;
        for(j=0;j<numevents;j++){
            int mask = 0;
            struct epoll_event *e = state->events + j;
            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
// 返回使用的API名称
static char *aeAPiName(void){
    return "epoll";
}