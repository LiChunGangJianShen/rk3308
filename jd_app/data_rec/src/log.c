/******************************************************************************
 * Copyright (C) 2014-2020 Zhifeng Gong <gozfree@163.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/
// #include <libposix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>
#include "log.h"
#include "color.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <fcntl.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <error.h> 
#include <errno.h>
#include "thread.h"

#define OS_LINUX

#if defined (OS_LINUX)
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/uio.h>
#ifndef __CYGWIN__
#include <sys/syscall.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

#define USE_SYSLOG

#elif defined (OS_ANDROID)
#include <jni.h>
#include <android/log.h>
#endif

#define LOG_IOVEC_MAX       (10)
#define FILENAME_LEN        (256)
#define FILESIZE_LEN        (500*1024UL)
#define LOG_BUF_SIZE        (1024)
#define LOG_TIME_SIZE       (32)
#define LOG_LEVEL_SIZE      (32)
#define LOG_TAG_SIZE        (32)
#define LOG_PNAME_SIZE      (32)
#define LOG_TEXT_SIZE       (256)
#define LOG_LEVEL_DEFAULT   LOG_INFO
#define LOG_IO_OPS

/*
 *#define LOG_VERBOSE_ENABLE
 */

#define LOG_PREFIX_MASK     (0xFFFF)
#define LOG_FULL_BIT        (1<<31)
#define LOG_TAG_BIT         (1<<3)
#define LOG_TIMESTAMP_BIT   (1<<2)
#define LOG_PIDTID_BIT      (1<<1)
#define LOG_FUNCLINE_BIT    (1<<0)
#define LOG_VERBOSE_BIT \
	(LOG_TIMESTAMP_BIT|LOG_PIDTID_BIT|LOG_FUNCLINE_BIT)

#define LOG_TAG_MASK        (0x0F)
#define LOG_TIMESTAMP_MASK  (0x07)
#define LOG_PIDTID_MASK     (0x03)
#define LOG_FUNCLINE_MASK   (0x01)

#define UPDATE_LOG_PREFIX(log, bit) \
    log |= (((bit) & LOG_TIMESTAMP_MASK) | \
            ((bit) & LOG_PIDTID_MASK) | \
            ((bit) & LOG_TAG_MASK) | \
            ((bit) & LOG_FUNCLINE_MASK))

#define CHECK_LOG_PREFIX(log, bit)  \
    ((log & LOG_PREFIX_MASK) & (bit))

#define level_str(x)    (x ? x : "err")
#define output_str(x)   (x ? x : "stderr")
#define time_str(x)     (x ? x : "0")

#define is_str_equal(a,b) \
    ((strlen(a) == strlen(b)) && (0 == strcasecmp(a,b)))

#ifdef __GNUC__
#define LIKELY(x)       (__builtin_expect(!!(x), 1))
#define UNLIKELY(x)     (__builtin_expect(!!(x), 0))
#else
#define LIKELY(x)       (x)
#define UNLIKELY(x)     (x)
#endif

#ifndef NAME_MAX
#define NAME_MAX         255 /* defined in /usr/include/linux/limits.h */
#endif

#ifndef PATH_SPLIT
#define PATH_SPLIT       '/'
#endif

typedef struct log_ops {
    int (*open)(const char *path);
    ssize_t (*write)(struct iovec *vec, int n);
    int (*close)(void);
} log_ops_t;

typedef struct log_driver {
    int (*init)(void);
    void (*deinit)();
} log_driver_t;

/* from /usr/include/sys/syslog.h */
static const char *_log_level_str[] = {
    "EMERG",
    "ALERT",
    "CRIT",
    "ERR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG",
    "VERBO",
    NULL
};


static int _is_log_init = 0;
static int _log_file_fd = 0;
static int _log_level = LOG_LEVEL_DEFAULT;
static char _log_path[FILENAME_LEN];
static char _log_name[FILENAME_LEN];
static char _log_name_prefix[FILENAME_LEN];
static char _log_name_time[FILENAME_LEN];
static pthread_mutex_t _log_mutex;
static int _log_prefix = 0;
static char _proc_name[NAME_MAX];
static unsigned long long _log_file_size = FILESIZE_LEN;
static int _log_file = 0;
static int _bind_cpu = 0;

static int _log_rotate = 0;
static int _log_color = 0;


int get_proc_name(char *name, size_t len)
{
    int i, ret;
    char proc_name[PATH_MAX];
    char *ptr = NULL;
    memset(proc_name, 0, sizeof(proc_name));
    if (-1 == readlink("/proc/self/exe", proc_name, sizeof(proc_name))) {
        fprintf(stderr, "readlink failed!\n");
        return -1;
    }
    ret = strlen(proc_name);
    for (i = ret, ptr = proc_name; i > 0; i--) {
        if (ptr[i] == PATH_SPLIT) {
            ptr+= i+1;
            break;
        }
    }
    if (i == 0) {
        fprintf(stderr, "proc path %s is invalid\n", proc_name);
        return -1;
    }
    if (ret-i > (int)len) {
        fprintf(stderr, "proc name length %d is larger than %d\n", ret-i, (int)len);
        return -1;
    }
    strncpy(name, ptr, ret - i);
    return 0;
}

static unsigned long long get_file_size(const char *path)
{
    struct stat buf;
    if (stat(path, &buf) < 0) {
        return 0;
    }
    return (unsigned long long)buf.st_size;
}

#if defined (OS_APPLE) || defined (OS_RTOS)
static pid_t gettid(void)
{
    return 0;
}

#elif defined (OS_LINUX)
static pid_t gettid(void)
{
#ifndef __CYGWIN__
    return syscall(__NR_gettid);
#else
    return 0;
#endif
}
#endif

static void log_get_time(char *str, int len, int flag_name)
{
    char date_fmt[20];
    char date_ms[32];
    struct timeval tv;
    struct tm now_tm;
    int now_ms;
    time_t now_sec;
    gettimeofday(&tv, NULL);
    now_sec = tv.tv_sec;
    now_ms = tv.tv_usec/1000;
    localtime_r(&now_sec, &now_tm);

    if (flag_name == 0) {
        strftime(date_fmt, 20, "%Y-%m-%d %H:%M:%S", &now_tm);
        snprintf(date_ms, sizeof(date_ms), "%03d", now_ms);
        snprintf(str, len, "[%s.%s]", date_fmt, date_ms);
    } else {
        strftime(date_fmt, 20, "%Y_%m_%d_%H_%M_%S", &now_tm);
        snprintf(date_ms, sizeof(date_ms), "%03d", now_ms);
        snprintf(str, len, "%s_%s.log", date_fmt, date_ms);
    }
}

static const char *get_dir(const char *path)
{
    char *p = (char *)path + strlen(path);
    for (; p != path; p--) {
       if (*p == '/') {
           *(p + 1) = '\0';
           break;
       }
    }
    return path;
}

static int mkdir_r(const char *path, mode_t mode)
{
    int ret = 0;
    char *temp, *pos;
    if (!path) {
        return -1;
    }

    temp = strdup(path);
    pos = temp;

    if (strncmp(temp, "/", 1) == 0) {
        pos += 1;
    } else if (strncmp(temp, "./", 2) == 0) {
        pos += 2;
    }
    for ( ; *pos != '\0'; ++ pos) {
        if (*pos == '/') {
            *pos = '\0';
            if (-1 == (ret = mkdir(temp, mode))) {
                if (errno == EEXIST) {
                    ret = 0;
                } else {
                    fprintf(stderr, "failed to mkdir %s: %d:%s\n",
                                    temp, errno, strerror(errno));
                    break;
                }
            }
            *pos = '/';
        }
    }
    if (*(pos - 1) != '/') {
        // fprintf(stderr, "mkdir_r if %s\n", temp);
        if (-1 == (ret = mkdir(temp, mode))) {
            if (errno == EEXIST) {
                ret = 0;
            } else {
                fprintf(stderr, "failed to mkdir %s: %d:%s\n",
                                temp, errno, strerror(errno));
            }
        }
    }
    free(temp);
    return ret;
}

static void check_dir(const char *path)
{
    char *path_org = NULL;
    const char *dir = NULL;
    if (strstr(path, "/")) {//file with dir
        path_org = strdup(path);
        dir = get_dir(path_org);
        if (-1 == access(dir, F_OK|W_OK|R_OK)) {
            if (-1 == mkdir_r(dir, 0775)) {
                fprintf(stderr, "mkdir %s failed\n", path_org);
            }
        }
        free(path_org);
    }
}

static int _log_open(const char *path)
{
    check_dir(path);
    _log_file_fd = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
    if (_log_file_fd == -1) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
    }
    return 0;
}

static int _log_open_rewrite(const char *path)
{
    check_dir(path);
    _log_file_fd = open(path, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (_log_file_fd == -1) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
    }
    return 0;
}
static int _log_close(void)
{
    return close(_log_file_fd);
}


int remove_old_file(char *log_file)
{
#define LOG_FILE_MAX 4
    DIR  *pdir;
    struct dirent * pdirent;
    struct stat f_ftime;
    int fcnt, i;
    char tmp[512] = {0};
    char files[LOG_FILE_MAX][FILENAME_LEN*3] = {0};
    char log_name[FILENAME_LEN*3] = {0};

    strncpy(tmp, log_file, sizeof(tmp));
    get_dir(tmp);

    // printf("dir=%s\n", tmp);
    pdir = opendir(tmp);
    if(pdir==NULL) {
        return 0;
    }
    
    // printf("read dir=%s\n", tmp);
    fcnt=0;
    for(pdirent=readdir(pdir); pdirent!=NULL; pdirent=readdir(pdir))
    {
        if(strcmp(pdirent->d_name,".")==0|| strcmp(pdirent->d_name,"..")==0) {
            continue;
        }
        if(S_ISDIR(f_ftime.st_mode)) {
            continue;
        }
        
        if(!strstr(pdirent->d_name, ".log")) {
            continue;
        }
        printf("%s file:%s\n", __func__, pdirent->d_name);
        strncpy(files[fcnt], pdirent->d_name, sizeof(files[fcnt]));

        fcnt++;
        if(fcnt >= LOG_FILE_MAX) {
            break;
        }
    }
    closedir(pdir);
    // printf("%s fcnt:%d\n", __func__, fcnt);

    if(fcnt > 2) {
        strncpy(log_name, files[0], sizeof(log_name));
        // printf("%s log_name:%s\n", __func__, log_name);

        for(i=1; i<fcnt; i++) {
            if(strcmp(files[i], log_name) < 0) {
                strncpy(log_name, files[i], sizeof(log_name));
            }
        }

        snprintf(tmp, sizeof(tmp), "%s%s", _log_path, log_name);
        // printf("%s delete file:%s\n", __func__, tmp);
        unlink(tmp);
    }

    return fcnt;
}

static ssize_t _log_write(struct iovec *vec, int n)
{
    char log_rename[FILENAME_LEN*3] = {0};
    unsigned long long tmp_size = get_file_size(_log_name);
    if (UNLIKELY(tmp_size > _log_file_size)) {
        if (_log_rotate) {
            if (-1 == _log_close()) {
                fprintf(stderr, "_log_close errno:%d", errno);
            }
            _log_open_rewrite(_log_name);
        } else {
            // fprintf(stderr, "%s size= %" PRIu64 " reach max %" PRIu64 ", splited\n",
                    // _log_name, (uint64_t)tmp_size, (uint64_t)_log_file_size);
            if (-1 == _log_close()) {
                fprintf(stderr, "_log_close errno:%d", errno);
            }
            log_get_time(_log_name_time, sizeof(_log_name_time), 1);
            snprintf(log_rename, sizeof(log_rename), "%s%s_%s",
                    _log_path, _log_name_prefix, _log_name_time);
            if (-1 == rename(_log_name, log_rename)) {
                fprintf(stderr, "log file splited %s error: %d:%s\n",
                        log_rename, errno , strerror(errno));
            }
            while(remove_old_file(_log_name) > 2);
            _log_open(_log_name);
            // fprintf(stderr, "splited file %s\n", log_rename);
        }
    }

    return writev(_log_file_fd, vec, n);
}


static struct log_ops log_io_ops = {
    _log_open,
    _log_write,
    _log_close
};

static struct log_ops *_log_handle = NULL;
static int relay_socket_pair[2] = {0};
pthread_state_t relay_thread_data = {0};

static int relay_thread(void *arg)
{
    struct iovec vec[1];
    char s_msg[LOG_BUF_SIZE] = {0};

    vec[0].iov_base = s_msg;
    vec[0].iov_len = sizeof(s_msg);

    while(_log_handle) {        
        if(readv(relay_socket_pair[1], vec, 1) > 0) {
            if(!relay_thread_data.running && !strcmp(vec[0].iov_base, "quit")) {
                break;
            }

            vec[0].iov_len = strlen(s_msg);
            _log_handle->write(vec, 1);

            memset(s_msg, 0, sizeof(s_msg));
            vec[0].iov_len = sizeof(s_msg);
        } 
    }

    return 0;
}

static void relay_write(struct iovec *vec, int n)
{
    int fd = relay_socket_pair[0];

    if(fd) {
        writev(fd, vec, n);
    }
}

static void relay_init(int bind_cpu)
{
    if(socketpair(AF_UNIX, SOCK_DGRAM, 0, relay_socket_pair) == -1 ) { 
        fprintf(stderr, "Error, socketpair create failed, errno(%d): %s\n", errno, strerror(errno));
        return; 
    }

    create_thread("log_thread", bind_cpu, FALSE, relay_thread, &relay_thread_data);
}



static void relay_exit(void)
{
    struct iovec vec[1];

    destroy_notice_thread(&relay_thread_data);
    vec[0].iov_base = "quit";
    vec[0].iov_len = strlen("quit");
    relay_write(vec, 1);
    destroy_thread(&relay_thread_data);

    close(relay_socket_pair[0]);
    close(relay_socket_pair[1]);

    relay_socket_pair[0] = 0;
    relay_socket_pair[1] = 0;
}

/*
 *time: level: process[pid]: [tid] tag: message
 *             [verbose          ]
 */
static int _log_print(int lvl, const char *tag,
                      const char *file, int line,
                      const char *func, const char *msg)
{
    int ret = 0, i = 0;
    struct iovec vec[LOG_IOVEC_MAX];
    char s_time[LOG_TIME_SIZE];
    char s_lvl[LOG_LEVEL_SIZE];
    char s_lvl_no_color[LOG_LEVEL_SIZE];
    char s_tag[LOG_TAG_SIZE];
    char s_pname[LOG_PNAME_SIZE*2];
    char s_pid[LOG_PNAME_SIZE];
    char s_tid[LOG_PNAME_SIZE];
    char s_file[LOG_TEXT_SIZE];
    char s_msg[LOG_BUF_SIZE];
    char s_msg_no_color[LOG_BUF_SIZE];
    int i_lvl = 0, i_msg = 0;

    pthread_mutex_lock(&_log_mutex);
    log_get_time(s_time, sizeof(s_time), 0);

    if(_log_color) {
        switch(lvl) {
        case LOG_EMERG:
        case LOG_ALERT:
        case LOG_CRIT:
        case LOG_ERR:
            snprintf(s_lvl, sizeof(s_lvl),
                    B_RED("[%5s]"), _log_level_str[lvl]);
            snprintf(s_msg, sizeof(s_msg), RED("%s"), msg);
            break;
        case LOG_WARNING:
            snprintf(s_lvl, sizeof(s_lvl),
                    B_YELLOW("[%5s]"), _log_level_str[lvl]);
            snprintf(s_msg, sizeof(s_msg), YELLOW("%s"), msg);
            break;
        case LOG_INFO:
            snprintf(s_lvl, sizeof(s_lvl),
                    B_GREEN("[%5s]"), _log_level_str[lvl]);
            snprintf(s_msg, sizeof(s_msg), GREEN("%s"), msg);
            break;
        case LOG_DEBUG:
            snprintf(s_lvl, sizeof(s_lvl),
                    B_WHITE("[%5s]"), _log_level_str[lvl]);
            snprintf(s_msg, sizeof(s_msg), WHITE("%s"), msg);
            break;
        default:
            snprintf(s_lvl, sizeof(s_lvl),
                    "[%5s]", _log_level_str[lvl]);
            snprintf(s_msg, sizeof(s_msg), "%s", msg);
            break;
        }
    }
    
    switch(lvl) {
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
        snprintf(s_lvl_no_color, sizeof(s_lvl_no_color),
                ("[%5s]"), _log_level_str[lvl]);
        snprintf(s_msg_no_color, sizeof(s_msg_no_color), ("%s"), msg);
        break;
    case LOG_WARNING:
        snprintf(s_lvl_no_color, sizeof(s_lvl_no_color),
                ("[%5s]"), _log_level_str[lvl]);
        snprintf(s_msg_no_color, sizeof(s_msg_no_color), ("%s"), msg);
        break;
    case LOG_INFO:
        snprintf(s_lvl_no_color, sizeof(s_lvl_no_color),
                ("[%5s]"), _log_level_str[lvl]);
        snprintf(s_msg_no_color, sizeof(s_msg_no_color), ("%s"), msg);
        break;
    case LOG_DEBUG:
        snprintf(s_lvl_no_color, sizeof(s_lvl_no_color),
                ("[%5s]"), _log_level_str[lvl]);
        snprintf(s_msg_no_color, sizeof(s_msg_no_color), ("%s"), msg);
        break;
    default:
        snprintf(s_lvl_no_color, sizeof(s_lvl_no_color),
                "[%5s]", _log_level_str[lvl]);
        snprintf(s_msg_no_color, sizeof(s_msg_no_color), "%s", msg);
        break;
    }


    if (CHECK_LOG_PREFIX(_log_prefix, LOG_PIDTID_BIT)) {
        snprintf(s_pname, sizeof(s_pname), "[%s ", _proc_name);
        snprintf(s_pid, sizeof(s_pid), "pid:%d ", getpid());
        snprintf(s_tid, sizeof(s_tid), "tid:%d]", (int)gettid());
        snprintf(s_tag, sizeof(s_tag), "[%s]", tag);
        snprintf(s_file, sizeof(s_file), "[%s:%3d: %s] ", file, line, func);
    }
    if (CHECK_LOG_PREFIX(_log_prefix, LOG_FUNCLINE_BIT)) {
        snprintf(s_file, sizeof(s_file), "[%s:%3d: %s] ", file, line, func);
    }

    i = -1;
    if (CHECK_LOG_PREFIX(_log_prefix, LOG_TIMESTAMP_BIT)) {
        vec[++i].iov_base = (void *)s_time;
        vec[i].iov_len = strlen(s_time);
    }
    if (CHECK_LOG_PREFIX(_log_prefix, LOG_PIDTID_BIT)) {
        vec[++i].iov_base = (void *)s_pname;
        vec[i].iov_len = strlen(s_pname);
        vec[++i].iov_base = (void *)s_pid;
        vec[i].iov_len = strlen(s_pid);
        vec[++i].iov_base = (void *)s_tid;
        vec[i].iov_len = strlen(s_tid);
    }
    
    if(_log_color) {
        vec[++i].iov_base = (void *)s_lvl;
        vec[i].iov_len = strlen(s_lvl);
    } else {
        vec[++i].iov_base = (void *)s_lvl_no_color;
        vec[i].iov_len = strlen(s_lvl_no_color);
    }
    i_lvl = i;

    if (CHECK_LOG_PREFIX(_log_prefix, LOG_TAG_BIT)) {
        vec[++i].iov_base = (void *)s_tag;
        vec[i].iov_len = strlen(s_tag);
    }
    if (CHECK_LOG_PREFIX(_log_prefix, LOG_FUNCLINE_BIT)) {
        vec[++i].iov_base = (void *)s_file;
        vec[i].iov_len = strlen(s_file);
    }

    if(_log_color) {
        vec[++i].iov_base = (void *)s_msg;
        vec[i].iov_len = strlen(s_msg);
    } else {
        vec[++i].iov_base = (void *)s_msg_no_color;
        vec[i].iov_len = strlen(s_msg_no_color);
    }
    i_msg = i;

    // ret = _log_handle->write(vec, i+1);
    writev(STDERR_FILENO, vec, i+1);

    if(_log_file) {
        if(_log_color) {
            vec[i_lvl].iov_base = (void *)s_lvl_no_color;
            vec[i_lvl].iov_len = strlen(s_lvl_no_color);

            vec[i_msg].iov_base = (void *)s_msg_no_color;
            vec[i_msg].iov_len = strlen(s_msg_no_color);
        }

        relay_write(vec, i+1);
    }

    pthread_mutex_unlock(&_log_mutex);
    return ret;
}

#ifdef __ANDROID__

#undef loge
#define loge(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#undef logw
#define logw(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#undef logi
#define logi(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#undef logd
#define logd(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#undef logv
#define logv(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

#else
int log_print(int lvl, const char *tag, const char *file,
              int line, const char *func, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE] = {0};
    int n, ret;

    if (UNLIKELY(!_is_log_init)) {
        log_init(_bind_cpu, 0, "./", "unknow");
    }

    if (lvl > _log_level) {
        return 0;
    }

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (UNLIKELY(n < 0)) {
        fprintf(stderr, "vsnprintf errno:%d\n", errno);
        return -1;
    }

    ret = _log_print(lvl, tag, file, line, func, buf);

    return ret;
}
#endif

void log_set_level(int level)
{
    if (level > LOG_VERB || level < LOG_EMERG) {
        _log_level = LOG_LEVEL_DEFAULT;
    } else {
        _log_level = level;
    }
}

static void log_check_env(int *lvl)
{
    const char *levelstr = level_str(getenv(LOG_LEVEL_ENV));
    const char *timestr = time_str(getenv(LOG_TIMESTAMP_ENV));
    const char *colorstr = time_str(getenv(LOG_COLOR_ENV));
    int level = atoi(levelstr);
    int timestamp = atoi(timestr);
    *lvl = LOG_LEVEL_DEFAULT;

    
    if (colorstr && is_str_equal(colorstr, "enable")) {
        _log_color = 1;
    }

    switch (level) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        *lvl = level;
        break;
    case 0:
        if (is_str_equal(levelstr, "error")) {
            *lvl = LOG_ERR;
        } else if (is_str_equal(levelstr, "warn")) {
            *lvl = LOG_WARNING;
        } else if (is_str_equal(levelstr, "notice")) {
            *lvl = LOG_NOTICE;
        } else if (is_str_equal(levelstr, "info")) {
            *lvl = LOG_INFO;
        } else if (is_str_equal(levelstr, "debug")) {
            *lvl = LOG_DEBUG;
        } else if (is_str_equal(levelstr, "verbose")) {
            *lvl = LOG_VERB;
        }
        break;
    default:
        break;
    }
    switch (timestamp) {
    case 1:
        UPDATE_LOG_PREFIX(_log_prefix, LOG_TIMESTAMP_BIT);
        break;
    case 0:
        if (is_str_equal(timestr, "y") ||
            is_str_equal(timestr, "yes") ||
            is_str_equal(timestr, "true")) {
             UPDATE_LOG_PREFIX(_log_prefix, LOG_TIMESTAMP_BIT);
        }
        break;
    default:
        break;
    }
    if (*lvl == LOG_DEBUG) {
        UPDATE_LOG_PREFIX(_log_prefix, LOG_FUNCLINE_BIT);
    }
    if (*lvl == LOG_VERB) {
        UPDATE_LOG_PREFIX(_log_prefix, LOG_VERBOSE_BIT);
    }
}

void log_set_split_size(int size)
{
    if ((uint32_t)size > FILESIZE_LEN || size < 0) {
        _log_file_size = FILESIZE_LEN;
    } else {
        _log_file_size = size;
    }
}

void log_set_rotate(int enable)
{
    _log_rotate = enable;
}

int log_set_path(const char *path)
{
    if (!path) {
        fprintf(stderr, "invalid path!\n");
        return -1;
    }
    if (strlen(path) == 0) {
        fprintf(stderr, "invalid path!\n");
        return -1;
    }
    strncpy(_log_path, path, sizeof(_log_path));
    return 0;
}


static int log_init_file(void)
{
    snprintf(_log_name, sizeof(_log_name), "%s/%s.log", _log_path, _log_name_prefix);
    
    _log_file_fd = 0;
    _log_handle->open(_log_name);
    return 0;
}

static void log_deinit_file(void)
{
    _log_handle->close();
}


static struct log_driver log_file_driver = {
    log_init_file,
    log_deinit_file,
};

pthread_once_t thread_once = PTHREAD_ONCE_INIT;

static void log_init_once(void)
{
    if (_is_log_init) {
        return;
    }
    log_check_env(&_log_level);
#ifdef LOG_VERBOSE_ENABLE
    UPDATE_LOG_PREFIX(_log_prefix, LOG_VERBOSE_BIT);
#endif
    UPDATE_LOG_PREFIX(_log_prefix, LOG_FUNCLINE_BIT);

    _log_handle = &log_io_ops;
    
    if (CHECK_LOG_PREFIX(_log_prefix, LOG_VERBOSE_BIT)) {
        memset(_proc_name, 0, sizeof(_proc_name));
        if (get_proc_name(_proc_name, sizeof(_proc_name))) {
            fprintf(stderr, "get_proc_name failed\n");
        }
    }

    if(_log_file) {
        log_file_driver.init();
        relay_init(_bind_cpu);
    }

    _is_log_init = 1;
    pthread_mutex_init(&_log_mutex, NULL);
    return;
}

int log_init(int bind_cpu, int log_file, const char *dir, const char *name)
{    
    strncpy(_log_path, dir?dir:"./", sizeof(_log_path));
    strncpy(_log_name_prefix, name?name:"unknow", sizeof(_log_name_prefix)); 

    _bind_cpu = bind_cpu;
    _log_file = log_file;

    if (0 != pthread_once(&thread_once, log_init_once)) {
        fprintf(stderr, "pthread_once failed\n");
    }
    return 0;
}

void log_deinit(void)
{
    if (!_is_log_init) {
        return;
    }

    if(_log_file) {
        relay_exit();
        log_file_driver.deinit();
    }

    _is_log_init = 0;
    pthread_mutex_destroy(&_log_mutex);
}
