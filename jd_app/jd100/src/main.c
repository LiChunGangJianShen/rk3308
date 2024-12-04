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
#include "led_ctrl.h"
#include "rk3308_soc.h"
#include "audio.h"
#include "signal_handle.h"
#include "thread.h"
#include "comm.h"
#include "API.h"
#include "build_time.h"
#include "app_version.h"

#define CPU_0   (0)
#define CPU_1   (1)
#define CPU_2   (2)
#define CPU_3   (3)

static int thread_running = TRUE;

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

int main(int argc, char **argv)
{
    char path[PATH_MAX] = {0};
    char process_name[32] = {0};
    char info[256];

    get_executable_path(path, process_name, PATH_MAX);
    logi("%s start\n", process_name);
    signal_hanler_init(process_name, &thread_running);
    snprintf(info, sizeof(info), "\n\nAPP: %s\nbuild: %s\napp:   %s\nalgo:  %s\n\n", process_name, BUILD_TIME, APP_VERSION, JD_MicArray_GetVersion());
    logd("%s", info);
    write_file_str("/tmp/version", info);
#if ENABLE_ALGO
    bool algo_init1 = false;
    bool algo_init2 = false;
    bool algo_init3 = false;
    algo_init1 = JD_MicArray_Init1();
    if(algo_init1 == false){
        loge("JD_MicArray_Init1 error\n");
        goto err_exit;
    }
    algo_init2 = JD_MicArray_Init2();
    if(algo_init2 == false){
        loge("JD_MicArray_Init2 error\n");
        goto err_exit;
    }
    algo_init3 = JD_MicArray_Init3();
    if(algo_init3 == false){
        loge("JD_MicArray_Init3 error\n");
        goto err_exit;
    }
    logi("---- algo init success ----\n");
#endif
    _sem_init();
    init_rngbuff();

    alg_task_init(CPU_1);
    alg2_task_init(CPU_2);
    alg3_task_init(CPU_3);
    rec_task_init(CPU_0);
    capture_task_init(CPU_0);
    playback_task_init(CPU_0);
    led_ctrl_task_init(CPU_0);

    while(thread_running){
        sleep(1);
    }

    alg_task_exit();
    alg2_task_exit();
    alg3_task_exit();
    rec_task_exit();
    capture_task_exit();
    playback_task_exit();
    led_ctrl_task_exit();

    _sem_destroy();
    exit_rngbuff();

#if ENABLE_ALGO
err_exit:
    if(algo_init1){
        JD_MicArray_Close1();
    }
    if(algo_init2){
        JD_MicArray_Close2();
    }
    if(algo_init3){
        JD_MicArray_Close3();
    }
#endif
    signal_hanler_exit();
    logi("%s stop\n", process_name);

    return 0;
}