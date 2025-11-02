#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EN_DEBUG    1

// 从完整路径中提取文件名部分
#define BASENAME(filepath) \
    (strrchr(filepath, '/') ? strrchr(filepath, '/') + 1 : filepath)

#if EN_DEBUG
// 带调试信息的打印宏
#define DEBUG_PRINT(fmt, ...) \
    printf("[%s:%d %s] " fmt, BASENAME(__FILE__), __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#ifdef __cplusplus  
}
#endif

#endif