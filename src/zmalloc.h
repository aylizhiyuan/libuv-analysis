/*
 * @Author: lizhiyuan
 * @Date: 2021-01-07 15:12:04
 * @LastEditors: lizhiyuan
 * @LastEditTime: 2021-01-08 14:15:46
 */
#ifndef _ZMALLOC_H
#define _ZMALLOC_H

#ifndef zmalloc
#define zmalloc malloc
#endif

#ifndef zfree
#define zfree free
#endif

#ifndef zrealloc
#define zrealloc realloc
#endif

#endif 