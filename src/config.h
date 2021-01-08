/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:45
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-08 14:26:07
 */
#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

/* Test for polling API */
#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1
#endif

#ifdef __sun
#include <sys/feature_tests.h>
#ifdef _DTRACE_VERSION
#define HAVE_EVPORT 1
#endif
#endif


#endif