#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "log.h"
#include "rk3308_soc.h"
#include "audio.h"
#include "signal_handle.h"
#include "thread.h"
#include "comm.h"
#include "API.h"
#include "build_time.h"
#include "app_version.h"
#include "rkgpio.h"

#define CPU_0   (0)
#define CPU_1   (1)
#define CPU_2   (2)
#define CPU_3   (3)

static int thread_running = 1;

static void write_file_str(const char *file, const char *str)
{
	FILE *fp;
	
	if(!file || !str) {
		return;
	}

	fp = fopen(file, "w");
	if(fp) {
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	} else {
		loge("write file=%s fail!!!\n", file);
	}
}

static void init_led_run(void)
{
    rk_set_gpio_export(0, 'C', 4);
    rk_set_gpio_direction_out(0, 'C', 4);
    rk_set_gpio_value(0, 'C', 4, 0);
}

static void exit_led_run(void)
{
    rk_set_gpio_value(0, 'C', 4, 0);
    rk_set_gpio_unexport(0, 'C', 4);
}

static void led_run_state(int state)
{
    if(state)
        rk_set_gpio_value(0, 'C', 4, 1);
    else
        rk_set_gpio_value(0, 'C', 4, 0);
}

static void init_ad_reset(void)
{
    rk_set_gpio_export(0, 'A', 3);
    rk_set_gpio_direction_out(0, 'A', 3);
    rk_set_gpio_value(0, 'A', 3, 1);
}

static void exit_ad_reset(void)
{
    rk_set_gpio_value(0, 'A', 3, 0);
    rk_set_gpio_unexport(0, 'A', 3);
}

int main(int argc, char **argv)
{
    char path[PATH_MAX] = {0};
    char process_name[32] = {0};
    char info[256];

    get_executable_path(path, process_name, PATH_MAX);
    logi("%s start\n", process_name);
    signal_hanler_init(process_name, &thread_running);
    snprintf(info, sizeof(info), "\n\nAPP: %s\nbuild: %s\napp:   %s\nalgo:  %s\n\n", process_name, BUILD_TIME, APP_VERSION, JDZH_FeedbackDestroy_GetVersion());
    logi("%s", info);
    write_file_str("/tmp/version", info);
#if ENABLE_ALGO
    bool algo_init1 = false;
    bool algo_init2 = false;
    bool algo_init3 = false;
    algo_init1 = JDZH_FeedbackDestroy_Init1();
    if(algo_init1 == false){
        loge("JDZH_FeedbackDestroy_Init1 error\n");
        goto err_exit;
    }
    algo_init2 = JDZH_FeedbackDestroy_Init2();
    if(algo_init2 == false){
        loge("JDZH_FeedbackDestroy_Init2 error\n");
        goto err_exit;
    }
    algo_init3 = JDZH_FeedbackDestroy_Init3();
    if(algo_init3 == false){
        loge("JDZH_FeedbackDestroy_Init3 error\n");
        goto err_exit;
    }
    logi("---- algo init success ----\n");
#endif
    _sem_init();
    init_rngbuff();

    alg_task_init(CPU_1, 70);
    alg2_task_init(CPU_2, 70);
    alg3_task_init(CPU_3, 70);
    rec_task_init(CPU_0, 70);
    capture_task_init(CPU_0, 70);
    playback_task_init(CPU_0, 70);
    key_task_init(CPU_0, 69);
    serial_task_init(CPU_0, 69);

    int ad_reset = 0;
    int state = 0;
    init_led_run();
    while(thread_running){
		state = !state;
        led_run_state(state);
        sleep(1);
        if(!ad_reset){
            init_ad_reset();
            ad_reset = 1;
        }
    }

    if(ad_reset){
        exit_ad_reset();
    }
	exit_led_run();
    key_task_exit();
    serial_task_exit();
    alg_task_exit();
    alg2_task_exit();
    alg3_task_exit();
    rec_task_exit();
    capture_task_exit();
    playback_task_exit();

    _sem_destroy();
    exit_rngbuff();

#if ENABLE_ALGO
err_exit:
    if(algo_init1){
        JDZH_FeedbackDestroy_Close1();
    }
    if(algo_init2){
        JDZH_FeedbackDestroy_Close2();
    }
    if(algo_init3){
        JDZH_FeedbackDestroy_Close3();
    }
#endif
    logi("%s stop\n", process_name);
    signal_hanler_exit();

    return 0;
}