#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "algo.h"
#include "eq.h"
#include "log.h"

static float g_eq[EQ_BAND] = {0.0};
static const char *EQ_FILE = "/data/eq.conf";

static float extract_float(const char *input)
{
    // 查找第一个 ": " 的位置
    const char *colon_space = strstr(input, ": ");
    if (colon_space == NULL) {
        fprintf(stderr, "未找到 ': ' 分隔符\n");
        return 0.0f;
    }
    
    // 定位子字符串起始位置（跳过 ": "）
    const char *start = colon_space + 2;
    
    // 查找下一个空格的位置（作为子字符串结束）
    const char *end = strchr(start, ' ');
    
    // 计算子字符串长度
    size_t length;
    if (end != NULL) {
        length = end - start;
    } else {
        length = strlen(start); // 若没有空格则到字符串末尾
    }
    
    // 复制子字符串到临时缓冲区
    char temp[256];
    if (length >= sizeof(temp) - 1) { // 防止溢出
        length = sizeof(temp) - 1;
    }
    strncpy(temp, start, length);
    temp[length] = '\0'; // 确保终止符
    
    // 转换为float
    char *endptr;
    errno = 0; // 清除错误标志
    float value = strtof(temp, &endptr);
    
    // 错误检查
    if (endptr == temp) {
        fprintf(stderr, "无法转换数字: '%s'\n", temp);
        return 0.0f;
    }
    if (errno == ERANGE) {
        perror("数值超出float范围");
        return 0.0f;
    }
    
    return value;
}

int save_eq(float *eq, int len)
{
    int fd = 0;
    char buf[EQ_BAND][64] = {0};

    fd = open(EQ_FILE, O_CREAT|O_RDWR|O_TRUNC);
    if(fd < 0){
        loge("crate eq_file fail\n");
        return -1;
    }

    for (int i = 0; i < EQ_BAND; i++){
        sprintf(buf[i], "EQ%d: %.2f dB\n", i+1, g_eq[i]);
    }
    for(int i = 0; i < EQ_BAND; i++){
        // logi("%s\n", buf[i]);
        write(fd, buf[i], strlen(buf[i]));
    }

    return 0;
}

void algo_eq_init(void)
{
    char buf[64] = {0};
    FILE *fp = NULL;
    int i=0;
    if(access(EQ_FILE, F_OK) == 0){
        fp = fopen(EQ_FILE, "rw+");
        if(!fp){
            loge("fopen %s fail\n", EQ_FILE);
            return;
        }
        while(fgets(buf, sizeof(buf), fp) != NULL){
            if(i < EQ_BAND){
                g_eq[i] = extract_float(buf);
                i++;
            }
            else{
                loge("eq_file error\n");
                break;
            }
        }
        for(i = 0; i < EQ_BAND; i++){
            logi("last eq[%d]: %.2f\n", i+1, g_eq[i]);
        }
        _algo_set_eq(g_eq);
        fclose(fp);
    }
    else{
        fp = fopen(EQ_FILE, "w");
        if(!fp){
            loge("fopen %s fail\n", EQ_FILE);
            return;
        }
        // memset(g_eq, 0.0, sizeof(g_eq));
        save_eq(g_eq, sizeof(g_eq)/sizeof(g_eq[0]));
        for(i = 0; i < EQ_BAND; i++){
            logi("initial eq[%d]: %.2f\n", i+1, g_eq[i]);
        }
        _algo_set_eq(g_eq);
        fclose(fp);
    }
}

void check_eq(float eq_buf[], int len)
{
    char buf[64] = {0};
    FILE *fp = NULL;
    int i=0;
    if(access(EQ_FILE, F_OK) == 0){
        fp = fopen(EQ_FILE, "r");
        if(!fp){
            loge("fopen %s fail", EQ_FILE);
            return;
        }
        while(fgets(buf, sizeof(buf), fp) != NULL){
            if(i < EQ_BAND){
                g_eq[i] = extract_float(buf);
                i++;
            }
            else{
                loge("eq_file error\n");
                break;
            }
        }
        for(i = 0; i < EQ_BAND && i < len; i++){
            eq_buf[i] = g_eq[i];
            logi("get eq[%d]: %.2f dB\n", i+1, eq_buf[i]);
        }
        fclose(fp);
    }
}