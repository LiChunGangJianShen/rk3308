#ifndef _LOG_H_
#define _LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <syslog.h>

#define logi(fmt, args...)    syslog(LOG_INFO, "[%s-%d]"fmt, __func__, __LINE__, ##args)
#define loge(fmt, args...)    syslog(LOG_ERR, "[%s-%d]"fmt, __func__, __LINE__, ##args)
#define logw(fmt, args...)    syslog(LOG_WARNING, "[%s-%d]"fmt, __func__, __LINE__, ##args)
#define logd(fmt, args...)    syslog(LOG_DEBUG, "[%s-%d]"fmt, __func__, __LINE__, ##args)

#ifdef __cplusplus
}
#endif

#endif