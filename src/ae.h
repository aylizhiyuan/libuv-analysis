/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:04
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-18 16:25:10
 */
#ifndef __AE_H__
#define __AE_H__

#include <time.h>
#define AE_OK 0
#define AE_ERR -1
#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2
#define AE_BARRIER 4
#define AE_FILE_EVENTS 1
#define AE_TIME_EVENTS 2
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT 4
#define AE_CALL_AFTER_SLEEP 8
#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

#define AE_NOTUSED(v)((void) V)
struct aeEventLoop;

// 定义一种函数类型
typedef void aeFileProc(struct aeEventLoop *eventLoop,int fd,void *clientData,int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop,long long id,void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop,void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

// 定义一种结构体
typedef struct aeFileEvent {
    int mask;
    aeFileProc *rfileProc; // 指向函数的指针？
    aeFileProc *wfileProc;
    void *clientData;
} aeFileEvent;

/* Time event structure */
typedef struct aeTimeEvent {
    long long id; /* time event identifier. */
    long when_sec; /* seconds */
    long when_ms; /* milliseconds */
    aeTimeProc *timeProc;      
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    struct aeTimeEvent *prev;
    struct aeTimeEvent *next;
} aeTimeEvent;

/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;

/* State of an event based program */
typedef struct aeEventLoop {
    int maxfd;   /* 能注册的最大的文件数量 */
    int setsize; /* eventLoop的大小 */
    long long timeEventNextId;
    time_t lastTime;     /* Used to detect system clock skew */
    aeFileEvent *events; /* 注册的事件 */
    aeFiredEvent *fired; /* 发生读事件的文件 */
    aeTimeEvent *timeEventHead;
    int stop;
    void *apidata; /* epoll的实例 */
    aeBeforeSleepProc *beforesleep;
    aeBeforeSleepProc *aftersleep;
} aeEventLoop;

/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);
int aeProcessEvents(aeEventLoop *eventLoop, int flags);
int aeWait(int fd, int mask, long long milliseconds);
void aeMain(aeEventLoop *eventLoop);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);

#endif