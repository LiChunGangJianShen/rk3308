#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

char *get_process_name(char *buffer, size_t buffer_size) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return NULL;
    }
    exe_path[len] = '\0';

    // 复制到临时缓冲区以避免修改原始路径
    char path_copy[PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", exe_path);

    // 获取文件名部分
    char *name = basename(path_copy);

    // 可选：移除可能的" (deleted)"后缀（如果文件被删除）
    char *deleted_suffix = strstr(name, " (deleted)");
    if (deleted_suffix) {
        *deleted_suffix = '\0';
    }

    // 复制结果到提供的缓冲区
    strncpy(buffer, name, buffer_size);
    buffer[buffer_size - 1] = '\0'; // 确保字符串终止
    return buffer;
}

#ifdef __cplusplus
}
#endif