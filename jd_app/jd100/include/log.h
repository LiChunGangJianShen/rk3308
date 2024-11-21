#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define MY_LOG_INFO    1
#define MY_LOG_ERR     2
#define MY_LOG_WARN    3
#define MY_LOG_DBG     4

#define LOG_PRINT(level, format, ...)\
    do{\
        if(level <= MY_LOG_ERR){\
            fprintf(stderr, "[%s, %d] "format"\n", __func__, __LINE__, ##__VA_ARGS__);\
        } else{\
            fprintf(stdout, "[%s, %d] "format"\n", __func__, __LINE__, ##__VA_ARGS__);\
        }\
    }while(0);

#define log_info(format, ...)   LOG_PRINT(MY_LOG_INFO, format, ##__VA_ARGS__)
#define log_err(format, ...)    LOG_PRINT(MY_LOG_ERR, format, ##__VA_ARGS__)
#define log_warn(format, ...)   LOG_PRINT(MY_LOG_WARN, format, ##__VA_ARGS__)
#define log_dbg(format, ...)    LOG_PRINT(MY_LOG_DBG, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif