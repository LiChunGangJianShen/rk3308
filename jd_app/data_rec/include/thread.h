#ifndef __THREAD_H
#define __THREAD_H

#ifdef __cplusplus
extern "C" {
#endif


#define TRUE  (1)
#define FALSE (0)


typedef struct {
pthread_t id;
int running;
}pthread_state_t;

typedef int (*thread_func)(void *);

int create_thread(char *name, int bind_cpu,  int is_high_priority, thread_func run, pthread_state_t *thread_data);

void destroy_notice_thread(pthread_state_t *thread_data);

void destroy_thread(pthread_state_t *thread_data);


#ifdef __cplusplus
}
#endif

#endif
