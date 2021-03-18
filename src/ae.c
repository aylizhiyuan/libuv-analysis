/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:10:58
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-18 16:49:46
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
 * @description: 增加一个文件的监听
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
    // 1. 首先增加一个events数组,fd为数字
    aeFileEvent *fe = &eventLoop->events[fd];
    // 2. 将当前的文件fd添加到epoll/kqueue的监听列表中去,使用epoll_ctl / kevent
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

/**
 * @description: 删除一个文件的监听
 * @param {aeEventLoop} *eventLoop
 * @param {int} fd
 * @param {int} mask
 * @return {*}
 */
void aeDeleteFileEvent(aeEventLoop *eventLoop,int fd,int mask){
    if(fd >= eventLoop->setsize) return;
    // 1. 拿到文件events数组中的那个文件
    aeFileEvent *fe = &eventLoop->events[fd];
    if(fe->mask == AE_NONE) return;
    if(mask & AE_WRITABLE) mask |= AE_BARRIER;
    // 2. 从epoll/kqueue的监听列表中将此文件删除
    aeApiDelEvent(eventLoop,fd,mask);
    fe->mask = fe->mask & (~mask);
    if(fd == eventLoop->maxfd && fe->mask == AE_NONE){
        int j;
        for(j = eventLoop->maxfd -1;j>=0;j--){
            if(eventLoop->events[j].mask != AE_NONE) break;
        }
        eventLoop->maxfd = j;
    }
}

/**
 * @description: 
 * @param {aeEventLoop} *eventLoop
 * @param {int} fd
 * @return {*}
 */
int aeGetFileEvents(aeEventLoop *eventLoop,int fd){
    if(fd >= eventLoop->setsize) return 0;
    aeFileEvent *fe = &eventLoop->events[fd];
    return fe->mask;
}

/**
 * @description: 获取当前的时间
 * @param {long} *seconds
 * @param {long} *milliseconds
 * @return {*}
 */
static void aeGetTime(long *seconds,long *milliseconds){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    *seconds = tv.tv_sec; // 秒
    *milliseconds = tv.tv_usec/1000; // 毫秒
}
/**
 * @description: 在当前时间的基础上增加秒数或者是毫秒数
 * @param {longlong} milliseconds
 * @param {long} *sec
 * @param {long} *ms
 * @return {*}
 */
static void aeAddMillisecondsToNow(long long milliseconds,long *sec,long *ms){
   long cur_sec,cur_ms,when_sec,when_ms;
   aeGetTime(&cur_sec,&cur_ms); // 当前的时间
   // 这个应该是定时的时间吧....
   when_sec = cur_sec + milliseconds/1000; // 秒
   when_ms = cur_ms + milliseconds%1000; // 毫秒
   if(when_ms >= 1000){
       when_sec ++ ;
       when_ms -= 1000;
   }
   // 拿到的是当前时间的秒数
   *sec = when_sec;
   // 拿到的是当前时间的毫秒数
   // 当前时间 = when_sec + when_ms;
   *ms = when_ms;
}

/**
 * @description: 添加事件队列到链表中
 * @param {aeEventLoop} *eventLoop
 * @param {longlong} milliseconds
 * @param {aeTimeProc} *proc
 * @param {void} *clientData
 * @param {aeEventFinalizerProc} *finalizerProc
 * @return {*}
 */
long long aeCreateTimeEvent(aeEventLoop *eventLoop,long long milliseconds,aeTimeProc *proc,void *clientData,aeEventFinalizerProc *finalizerProc){
    long long id = eventLoop->timeEventNextId ++;
    aeTimeEvent *te;
    te = zmalloc(sizeof(*te));
    if(te == NULL) return AE_ERR;
    te->id = id;
    // 拿到定时时间的秒数te->when_sec,以及定时时间的毫秒数te->when_ms
    aeAddMillisecondsToNow(milliseconds,&te->when_sec,&te->when_ms);
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->prev = NULL;
    // 将所有的加入的定时任务组成一个双向的链表...
    te->next = eventLoop->timeEventHead;
    if(te->next){
        te->next->prev = te;
    }
    // 把当前定时任务的结构体放到timeEventhead上..
    // 任务1(Eventhead) ---> 任务2 ----> 任务3
    eventLoop->timeEventHead = te;
    return id;  
}
/**
 * @description: 将当前的id从事件链表中删除...
 * @param {aeEventLoop} *eventLoop
 * @param {longlong} id
 * @return {*}
 */
int adDeleteTimeEvent(aeEventLoop *eventLoop,long long id){
    aeTimeEvent *te = eventLoop->timeEventHead;
    while(te){
        if(te->id == id){
            // 如果发现了这个,将他的id赋值为-1
            te->id = AE_DELETED_EVENT_ID;
            return AE_OK;
        }
        te = te->next;
    }
    return AE_ERR;
}
/**
 * @description: 感觉这个应该是返回离得最近的定时任务
 * @param {aeEventLoop} *eventLoop
 * @return {*}
 */
static aeTimeEvent *aeSearchNearestTimer(aeEventLoop *eventLoop){
    aeTimeEvent *te = eventLoop->timeEventHead;
    aeTimeEvent *nearest = NULL;
    while(te){
        if (!nearest || te->when_sec < nearest->when_sec ||
                (te->when_sec == nearest->when_sec &&
                 te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}
/**
 * @description: 核心---处理时间的事件
 * @param {aeEventLoop} *eventLoop
 * @return {*}
 */
static int processTimeEvents(aeEventLoop *eventLoop){
    int processed = 0;
    aeTimeEvent *te;
    long long maxId;
    time_t now = time(NULL); // 获取当前的时间的秒数
    if(now < eventLoop->lastTime){
        // 拿到第一个定时任务
        te = eventLoop->timeEventHead;
        // 循环所有的定时任务
        while(te){
            te->when_sec = 0;
            te = te->next;
        }
    }
    eventLoop->lastTime = now; // 将启动定时任务的当前时间设置为loop的lastTime
    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId - 1;
    // 循环所有的定时任务
    while(te){
        long now_sec,now_ms;
        long long id;
        // 1. 将移除的定时任务从链表中删除
        if(te->id == AE_DELETED_EVENT_ID){
            aeTimeEvent *next = te->next;
            if(te->prev){
                te->prev->next = te->next;
            }else{
                eventLoop->timeEventHead = te->next;
            }
            if(te->next){
                te->next->prev = te->prev;
            }
            if(te->finalizerProc){
                te->finalizerProc(eventLoop, te->clientData);
            }
            zfree(te);
            te = next;
            continue;
        }
        // 此处可以暂时忽略掉....
        if(te->id > maxId){
            te = te->next;
            continue;
        }
        aeGetTime(&now_sec,&now_ms); // 获取当前时间的秒数和毫秒数
        // 当前的定时任务 == 当前时间或者超过当前时间的时候,可以考虑将当前的任务执行
        if(now_sec > te->when_sec || (now_sec == te->when_sec && now_ms >= te->when_ms)){
            int retval;
            id = te->id;
            // 什么时候这个timeProc会调用呢...
            // 我感觉这个就是所谓的回调函数吧...啊哈哈哈哈
            retval = te->timeProc(eventLoop,id,te->clientData);
            processed++;
            if(retval != AE_NOMORE){
                // 这个地方存疑.....感觉应该是这个回调执行完毕后没有正常返回-1的时候
                aeAddMillisecondsToNow(retval,&te->when_sec,&te->when_ms);
            }else{
                te->id = AE_DELETED_EVENT_ID; // 执行完毕后直接失效掉这个定时任务
            }
        }
        te = te->next;
    }
    return processed;
}
/**
 * @description: 
 * @param {aeEventLoop} *eventLoop
 * @param {int} flags
 * @return {*} 返回无需等待即可处理的事件
 */
// 处理每个挂起的时间事件,处理每个挂起的文件事件
// 标志设置了AE_ALL_EVENTS,则处理所有类型的事件
// 标志设置了AE_FILE_EVENTS,则处理文件事件
// 标志设置了AE_TIME_EVENtS,则处理时间事件
// 如果标志设置了AE_DONT_WAIT,则该函数将尽快返回
// 如果标志设置了AE_CALL_AFTER_SLEEP 则调用后睡眠回调
int aeProcessEvents(aeEventLoop *eventLoop,int flags){
    int processed = 0,numevents;
    if(!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;
    if(eventLoop->maxfd != 1 || ((flags & AE_TIME_EVENTS) && (flags & AE_DONT_WAIT))){
        
    }
    
    if(flags & AE_TIME_EVENTS){
        // 执行所有的定时任务回调函数....
        processed += processTimeEvents(eventLoop);
    }
    return processed;
}

/**
 * @description: 利用poll来完成阻塞等待....
 * @param {int} fd
 * @param {int} mask
 * @param {longlong} milliseconds
 * @return {*}
 */
int aeWait(int fd, int mask, long long milliseconds) {
    struct pollfd pfd;
    int retmask = 0, retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if (mask & AE_READABLE) pfd.events |= POLLIN;
    if (mask & AE_WRITABLE) pfd.events |= POLLOUT;

    if ((retval = poll(&pfd, 1, milliseconds))== 1) {
        if (pfd.revents & POLLIN) retmask |= AE_READABLE;
        if (pfd.revents & POLLOUT) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLERR) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLHUP) retmask |= AE_WRITABLE;
        return retmask;
    } else {
        return retval;
    }
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