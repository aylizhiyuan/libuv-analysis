/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:10:58
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-16 17:36:17
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
    if((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    // 不清楚这个是干嘛的....
    eventLoop->events = zmalloc(sizeof(aeFileEvent)*setsize);
    
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