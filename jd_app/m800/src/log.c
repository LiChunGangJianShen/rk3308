#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "log.h"
#include "comm.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IP_ADRR "127.0.0.1"
#define FILE_SIZE_MAX   (50*1024UL)
#define PORT 8888
static char g_log_path[256];
static pthread_state_t log_task_state;
static int g_sockfd = 0;
static pthread_mutex_t log_mutex;
static int g_default_log_level = LOG_DEFAULT_LEVEL;

int log_path_length();

static int udp_send(int sockfd, void *message, int len)
{
    int ret = 0;
    struct sockaddr_in target_addr;
    socklen_t addr_len = sizeof(target_addr);
    fd_set fds;
    struct timeval tval;

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP_ADRR, &target_addr.sin_addr);

    do{
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        tval.tv_sec = 0;
        tval.tv_usec = 10*1000;
    }while(select(sockfd + 1, NULL, &fds, NULL, &tval) <= 0);

    // printf("--- %s ---\n", __func__);
    ret = sendto(sockfd, message, len, 0,
              (struct sockaddr*)&target_addr, addr_len);

    return ret;
}

static int udp_recv(int sockfd, void *buffer, int len)
{
    int ret = 0;
    struct sockaddr_in target_addr;
    socklen_t addr_len = sizeof(target_addr);
    fd_set fds;
    struct timeval tval;

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP_ADRR, &target_addr.sin_addr);

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    tval.tv_sec = 0;
    tval.tv_usec = 10*1000;
    ret = select(sockfd + 1, &fds, NULL, NULL, &tval);
    if(ret > 0){
        ret = recvfrom(sockfd, buffer, len, 0,
            (struct sockaddr*)&target_addr, &addr_len);
    }
    return ret;
}

static int udp_init(const uint16_t port)
{
    int sockfd = 0;
    int reuse = 1;
    struct sockaddr_in server_addr;

    // 1. 创建Socket并绑定到本地端口
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        perror("socket creation failed");
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(sockfd);
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 绑定所有网卡
    server_addr.sin_port = htons(PORT);       // 绑定端口
    int bind_retry = 3;
    while(bind_retry > 0){
        if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            break;
        }
        perror("bind failed, retrying...");
        bind_retry--;
        sleep(1);
    }

    if(bind_retry == 0){
        perror("bind failed after retry\n");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

static void udp_delete(int fd)
{
    close(fd);
}

void log_print(int level, const char *file, const char *func, const int line, const char *format, ...) {
    time_t now;
    struct tm tm_info;
    char time_str[20];
    char all_log[1024];
    char buf[1024];
    // pid_t pid = getpid();
    const char *color_code = "";
    const char *prefix = "";
    int available = 0;

    if(level > g_default_log_level){
        return;
    }

    pthread_mutex_lock(&log_mutex);
    // 获取系统时间并格式化
    time(&now);
    localtime_r(&now, &tm_info);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

    // 根据日志等级设置颜色和前缀
    switch (level) {
        case LOG_ERR:
            color_code = "\033[31m";  // 红色
            prefix = "[Error] ";
            break;
        case LOG_WARN:
            color_code = "\033[33m";  // 黄色
            prefix = "[Warning] ";
            break;
        case LOG_DEBUG:
            prefix = "[Debug] ";
            break;
        case LOG_INFO:
            prefix = "[Info] ";
            break;
        default:
            prefix = "[Unknown] ";
            break;
    }

    // 输出日志头（时间、进程ID）
    // fprintf(stderr, "[%s] [%d] ", time_str, (int)pid);

    // char process_name[256];
    // get_process_name(process_name, sizeof(process_name));
    // fprintf(stderr, "[%s] [%s] [%s-%s-%d]", time_str, process_name, file, func, line);
    // memset(all_log, 0, sizeof(all_log));
    // snprintf(all_log, sizeof(all_log), "[%s] [%s] [%s-%s-%d]", time_str, process_name, file, func, line);

    char process_name[256];
    check_proc_name(process_name, sizeof(process_name));
    fprintf(stderr, "[%s] [%s] ", time_str, process_name);
    memset(all_log, 0, sizeof(all_log));
    snprintf(all_log, sizeof(all_log), "[%s] [%s] ", time_str, process_name);

    // 输出带颜色的前缀（仅限 ERR/WARN）
    if (color_code[0] != '\0') {
        fprintf(stderr, "%s%s", color_code, prefix);
        // available = sizeof(all_log) - strlen(all_log) - 1;
        // strncat(all_log, color_code, available);
        available = sizeof(all_log) - strlen(all_log) - 1;
        strncat(all_log, prefix, available);
    } else {
        fprintf(stderr, "%s", prefix);
        available = sizeof(all_log) - strlen(all_log) - 1;
        strncat(all_log, prefix, available);
    }

    char buf111[256];
    fprintf(stderr, "[%s-%s-%d] ", file, func, line);
    memset(buf111, 0, sizeof(buf111));
    snprintf(buf111, sizeof(buf111), "[%s-%s-%d] ", file, func, line);
    available = sizeof(all_log) - strlen(all_log) - 1;
    strncat(all_log, buf111, available);

    // 输出用户日志内容
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    vsnprintf(buf, sizeof(buf), format, args);
    available = sizeof(all_log) - strlen(all_log) - 1;
    strncat(all_log, buf, available);
    va_end(args);

    // 重置颜色并换行
    if (color_code[0] != '\0') {
        fprintf(stderr, "\033[0m");
    }
    fprintf(stderr, "\n");

    // 确保立即刷新输出
    fflush(stderr);
    udp_send(g_sockfd, all_log, strlen(all_log));
    pthread_mutex_unlock(&log_mutex);
}

static unsigned long long get_file_size(const char *path)
{
    if(!path){
        printf("%s invalid file path\n", __func__);
        return 0;
    }
    struct stat buf;
    if (stat(path, &buf) < 0) {
        return 0;
    }
    return (unsigned long long)buf.st_size;
}
static int log_file_reopen(const char *path)
{
    if(!path){
        printf("%s invalid file path\n", __func__);
        return 0;
    }
    int fd = open(path, O_CREAT|O_RDWR|O_TRUNC, 0644);
    if(fd < 0){
        printf("%s open %s fail\n", __func__, path);
    }

    return fd;
}
static int log_write(int fd, void *buf, int len)
{
    int ret = 0;

    if(get_file_size(g_log_path) >= FILE_SIZE_MAX){
        if(fd > 0){
            close(fd);
        }
        fd = log_file_reopen(g_log_path);
        if(fd < 0){
            printf("log file reopen fail\n");
            return 0;
        }
        ret = write(fd, buf, len);
    }
    else{
        ret = write(fd, buf, len);
    }

    return ret;
}

static int log_task(void *arg)
{
    char recv_buf[1024];
    int fd = 0;
    int ret = 0;

    printf("log file: %s\n", g_log_path);

    if(access(g_log_path, F_OK) == 0){
        fd = open(g_log_path, O_RDWR|O_APPEND);
        if(fd < 0){
            printf("open %s faild\n", g_log_path);
        }
    }
    else{
        fd = open(g_log_path, O_CREAT|O_RDWR, 0644);
        if(fd < 0){
            printf("create %s faild\n", g_log_path);
        }
    }

    g_sockfd = udp_init(PORT);
    while (log_task_state.running)
    {
        ret = udp_recv(g_sockfd, recv_buf, sizeof(recv_buf));
        log_write(fd, recv_buf, ret);
        usleep(100);
    }

    udp_delete(g_sockfd);
    close(fd);

    return 0;
}

int log_init(const int cpu_bind, int en_save, const char *dir, const char *log_name, int priority, int verbose)
{
    char buf[32]={0};

    pthread_mutex_init(&log_mutex, NULL);

    if(verbose){
        g_default_log_level = LOG_DEBUG;
    }

    if(dir){
        if(access(dir, F_OK) != 0){
            mkdir(dir, 0644);
        }
        snprintf(buf, sizeof(g_log_path), "%s/%s.log", dir, log_name);
        strncpy(g_log_path, buf, (log_path_length()>strlen(buf))?log_path_length()-1:strlen(buf));
    }
    else{
        if(access("/tmp/log", F_OK) != 0){
            mkdir("/tmp/log", 0644);
        }
        snprintf(buf, sizeof(g_log_path), "/tmp/log/%s.log", log_name);
        strncpy(g_log_path, buf, (log_path_length()>strlen(buf))?log_path_length()-1:strlen(buf));
    }

    // if(en_save){
        create_thread("log", cpu_bind, priority, log_task, &log_task_state);
    // }

    return 0;
}

void log_exit(void)
{
    destroy_thread(&log_task_state);
    pthread_mutex_destroy(&log_mutex);
}

int log_path_length()
{
    return sizeof(g_log_path)/sizeof(g_log_path[0]);
}

#ifdef __cplusplus
}
#endif