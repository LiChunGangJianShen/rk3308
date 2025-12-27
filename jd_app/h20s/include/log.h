#ifndef __LOG_H
#define __LOG_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_ERR   1
#define LOG_WARN  2
#define LOG_INFO  3
#define LOG_DEBUG 4
#define LOG_DEFAULT_LEVEL  LOG_INFO

int log_init(const int cpu_bind, int en_save, const char *dir, const char *log_name, int priority, int verbose);
void log_exit(void);
void log_print(int level, const char *file, const char *func, const int line, const char *format, ...);

#define BASENAME(filepath) \
    (strrchr(filepath, '/') ? strrchr(filepath, '/') + 1 : filepath)
#define loge(...)   log_print(LOG_ERR, BASENAME(__FILE__), __func__, __LINE__, __VA_ARGS__)
#define logw(...)   log_print(LOG_WARN, BASENAME(__FILE__), __func__, __LINE__, __VA_ARGS__)
#define logd(...)   log_print(LOG_DEBUG, BASENAME(__FILE__), __func__, __LINE__, __VA_ARGS__)
#define logi(...)   log_print(LOG_INFO, BASENAME(__FILE__), __func__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif