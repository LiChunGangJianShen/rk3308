#ifndef __THREAD_H
#define __THREAD_H

#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pthread_t id;
    bool running;
    char name[32];
}pthread_state_t;

typedef int (*thread_func)(void *);

int create_thread(const char *name, int bind_cpu,  int priority, thread_func run, pthread_state_t *thread_data);
void destroy_notice_thread(pthread_state_t *thread_data);
void destroy_thread(pthread_state_t *thread_data);

#ifdef __cplusplus
}
#endif

#endif
