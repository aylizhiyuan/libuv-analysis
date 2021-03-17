/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:10:58
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-17 14:21:08
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "ae.h"
#include "zmalloc.h"
#include "config.h"

#ifdef HAVE_EVPORT
#include "ae_evport.c"
#else
    #ifdef HAVE_EPOLL
    #include "ae_epoll.c"
    #else
        #ifdef HAVE_KQUEUE
        #include "ae_kqueue.c"
        #else
        #include "ae_select.c"
        #endif
    #endif
#endif

aeEventLoop *aeCreateEventLoop(int setsize){
    aeEventLoop *eventLoop;
    int i;
    // 1. eventLoop开辟空间
    if((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    // 2. eventLoop->events 注册的事件数组开辟空间
    eventLoop->events = zmalloc(sizeof(aeFileEvent)*setsize);
    // 3. eventLoop->fired 监听到的事件数组开辟空间
    eventLoop->fired = zmalloc(sizeof(aeFiredEvent)*setsize);
    if(eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    eventLoop->setsize = setsize;
    eventLoop->lastTime = time(NULL);
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    eventLoop->aftersleep =  NULL;
    // 创建epoll/kqueue的实例 = eventLoop->apidata =  实例
    if(aeApiCreate(eventLoop) == -1) goto err;
    for(i=0;i<setsize;i++){
        eventLoop->events[i].mask = AE_NONE; // 设置默认值
    }
    return eventLoop;
err:
    if(eventLoop){
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }    
    return NULL;
}

int aeGetSetSize(aeEventLoop *eventLoop){
    return eventLoop->setsize;
}

/**
 * @description: 重新设置eventLoop的大小
 * @param {aeEventLoop} *eventLoop
 * @param {int} setsize
 * @return {*}
 */
int aeResizeSetSize(aeEventLoop *eventLoop,int setsize){
    int i;
    if(setsize == eventLoop->setsize) return AE_OK;
    if(eventLoop->maxfd >= setsize) return AE_ERR;
    if(aeApiResize(eventLoop,setsize) == -1) return AE_ERR;
    eventLoop->events = zrealloc(eventLoop->events,sizeof(aeFileEvent)*setsize);
    eventLoop->fired = zrealloc(eventLoop->fired,sizeof(aeFiredEvent)*setsize);
    eventLoop->setsize = setsize;
    for (i = eventLoop->maxfd+1; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    return AE_OK;
}

void aeDeleteEventLoop(aeEventLoop *eventLoop){
    aeApiFree(eventLoop);
    zfree(eventLoop->events);
    zfree(eventLoop->fired);
    zfree(eventLoop);
}
void aeStop(aeEventLoop *eventLoop){
    eventLoop->stop = 1;
}
/**
 * @description: 
 * @param {aeEventLoop} *eventLoop
 * @param {int} fd
 * @param {int} mask
 * @param {aeFileProc} *proc
 * @param {void} *clientData
 * @return {*}
 */
int aeCreateFileEvent(aeEventLoop *eventLoop,int fd,int mask,aeFileProc *proc,void *clientData){
    // 如果超过了eventLoop的大小的话,直接报错...
    if(fd >= eventLoop->setsize){
        errno = ERANGE;
        return AE_ERR;
    }
    // 
    aeFileEvent *fe = &eventLoop->events[fd];
    // 将当前的文件fd添加到epoll/kqueue的监听列表中去,使用epoll_ctl / kevent
    if(aeApiAddEvent(eventLoop,fd,mask) == -1){
        return AE_ERR;
    }
    fe->mask |= mask;
    if(mask & AE_READABLE) fe->rfileProc = proc;
    if(mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if(fd > eventLoop->maxfd){
        eventLoop->maxfd = fd;
    }
    return AE_OK;
}



// 所有的异步一定是用一个特别的线程实现的,该线程专门用来处理
// 所有的异步任务.主线程等待异步线程执行结束后,再结束..
// 这里,关注一点,主线程中关于异步的回调函数是谁知道的,感觉应该是
// 子线程一旦完成了某个异步的任务,便会通知主线程,主线程将对应的回调函数执行了...
void aeMain(aeEventLoop *eventLoop){
    eventLoop->stop = 0;
    while(!eventLoop->stop){
        if(eventLoop->beforesleep != NULL){
            eventLoop->beforesleep(eventLoop);
        }
        aeProcessEvents(eventLoop,AE_ALL_EVENTS|AE_CALL_AFTER_SLEEP);
    }
}
char *aeGetApiName(void){
    return aeApiName();
}
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep) {
    eventLoop->beforesleep = beforesleep;
}

void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep) {
    eventLoop->aftersleep = aftersleep;
}