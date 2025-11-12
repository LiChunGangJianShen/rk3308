#ifndef LOG_H
#define LOG_H

#ifdef  __cplusplus
extern "C" {
#endif

#define _LOG_ERR     1
#define _LOG_WARN    2
#define _LOG_INFO    3
#define _LOG_DBG     4
#define _LOG_DEF     _LOG_INFO

#define LOG_PRINT(level, format, ...)\
    do{\
        if(level <= _LOG_DEF){\
            fprintf(stderr, "[%s, %d] "format"\n", __func__, __LINE__, ##__VA_ARGS__);\
        } else{\
            fprintf(stdout, "[%s, %d] "format"\n", __func__, __LINE__, ##__VA_ARGS__);\
        }\
    }while(0);

#define logi(format, ...)   LOG_PRINT(_LOG_INFO, format, ##__VA_ARGS__)
#define loge(format, ...)    LOG_PRINT(_LOG_ERR, format, ##__VA_ARGS__)
#define logw(format, ...)   LOG_PRINT(_LOG_WARN, format, ##__VA_ARGS__)
#define logd(format, ...)    LOG_PRINT(_LOG_DBG, format, ##__VA_ARGS__)

#ifdef  __cplusplus
}
#endif

#endif