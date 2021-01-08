/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:11:55
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-08 14:25:10
 */
#ifndef _REDIS_FMACRO_H
#define _REDIS_FMACRO_H

#define _BSD_SOURCE

#if defined(__liunx__)
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#endif

#if defined(__AIX)
#define _ALL_SOURCE
#endif

#if defined(__liunx__) || defined(__OpenBSD__)
#define _XOPEN_SOURCE 700
#elif !defined(__NetBSD__)
#define _XOPEN_SOURCE
#endif

#if defined(__sun)
#define __POSIX_C_SOURCE 199506L
#endif 

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif 