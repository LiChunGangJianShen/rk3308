#include <stdio.h>
#define _GUN_SOURCE
#define __USE_GNU
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include "thread.h"



static int api_get_thread_policy (pthread_attr_t *attr)
{
    int policy;
    int rs = pthread_attr_getschedpolicy (attr, &policy);
    assert (rs == 0);

    switch (policy)
    {
        case SCHED_FIFO:
            printf ("policy = SCHED_FIFO\n");
            break;
        case SCHED_RR:
            printf ("policy = SCHED_RR");
            break;
        case SCHED_OTHER:
            printf ("policy = SCHED_OTHER\n");
            break;
        default:
            printf ("policy = UNKNOWN\n");
            break; 
    }
    return policy;
}

static void api_show_thread_priority (pthread_attr_t *attr,int policy)
{
    int priority = sched_get_priority_max (policy);
    assert (priority != -1);
    printf ("max_priority = %d\n", priority);
    priority = sched_get_priority_min (policy);
    assert (priority != -1);
    printf ("min_priority = %d\n", priority);
}

static int api_get_thread_priority (pthread_attr_t *attr)
{
    struct sched_param param;
    int rs = pthread_attr_getschedparam (attr, &param);
    assert (rs == 0);
    printf ("priority = %d\n", param.__sched_priority);
    return param.__sched_priority;
}

static void api_set_thread_policy (pthread_attr_t *attr,int policy)
{
    int rs = pthread_attr_setschedpolicy (attr, policy);
    assert (rs == 0);
    api_get_thread_policy (attr);
}

int show_thread_priority(void)
{
    pthread_attr_t attr;       // 线程属性

    /* 
     * 对线程属性初始化
     * 初始化完成以后，pthread_attr_t 结构所包含的结构体
     * 就是操作系统实现支持的所有线程属性的默认值
     */
    pthread_attr_init (&attr);

    /* 获得当前调度策略 */
    int policy = api_get_thread_policy (&attr);

    /* 显示当前调度策略的线程优先级范围 */
    printf ("Show current configuration of priority\n");
    api_show_thread_priority(&attr, policy);

    /* 获取 SCHED_FIFO 策略下的线程优先级范围 */
    printf ("show SCHED_FIFO of priority\n");
    api_show_thread_priority(&attr, SCHED_FIFO);

    /* 获取 SCHED_RR 策略下的线程优先级范围 */
    printf ("show SCHED_RR of priority\n");
    api_show_thread_priority(&attr, SCHED_RR);

    /* 显示当前线程的优先级 */
    printf ("show priority of current thread\n");
    api_get_thread_priority (&attr);

    /* 手动设置调度策略 */
    printf ("Set thread policy\n");

    printf ("set SCHED_FIFO policy\n");
    api_set_thread_policy(&attr, SCHED_FIFO);

    printf ("set SCHED_RR policy\n");
    api_set_thread_policy(&attr, SCHED_RR);

    /* 还原之前的策略 */
    printf ("Restore current policy\n");
    api_set_thread_policy (&attr, policy);

    /* 
     * 反初始化 pthread_attr_t 结构
     * 如果 pthread_attr_init 的实现对属性对象的内存空间是动态分配的，
     * phread_attr_destory 就会释放该内存空间
     */
    pthread_attr_destroy (&attr);

    return 0;
}

static void set_thread_priority(pthread_attr_t *attr, int priority)
{
    struct sched_param param;
    
    pthread_attr_getschedparam(attr, &param);
    param.sched_priority = priority;

    pthread_attr_setschedpolicy(attr,SCHED_FIFO);
    pthread_attr_setschedparam(attr, &param);
    pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
}

int create_thread(char *name, int bind_cpu, int is_high_priority, thread_func run, pthread_state_t *thread_data)
{
	int ret = 0;
	pthread_attr_t attr;
	cpu_set_t cpu_set;
	
	if(!name || !run || !thread_data) {
        printf("create_thread fail, since input params is invalid\n");
		return -1;
	}

	if(bind_cpu > 3) {
        printf("create_thread fail, since input bind_cpu is invalid\n");
		return -1;
	}

	pthread_attr_init(&attr);
	CPU_ZERO(&cpu_set);
	CPU_SET(bind_cpu, &cpu_set);

    if(is_high_priority) {
        set_thread_priority(&attr, 70);
    }

	ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpu_set);
	if(0 != ret){
		printf("create_thread set affinity failed");
		return ret;
	}

    thread_data->running = TRUE;
	ret = pthread_create(&thread_data->id, &attr, (void *(*)(void *))run, thread_data);
	if(0 != ret){
		printf("create_thread create %s failed\n", name);
	}

	pthread_attr_destroy(&attr);
	return ret;
}

void destroy_notice_thread(pthread_state_t *thread_data)
{
    if(!thread_data) {
        printf("destroy_thread fail, since input param is invalid\n");
		return;
	}

    thread_data->running = FALSE;
}


void destroy_thread(pthread_state_t *thread_data)
{
    if(!thread_data) {
        printf("destroy_thread fail, since input param is invalid\n");
		return;
	}

    thread_data->running = FALSE;
    pthread_join(thread_data->id, NULL);
}


