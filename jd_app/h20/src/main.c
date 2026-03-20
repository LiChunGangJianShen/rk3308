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
#include "algo.h"
#include "build_time.h"
#include "app_version.h"
#include "run_led.h"
#include "ad_ctrl.h"

static int thread_running = 1;
static const char *ver_path = "/tmp/version";

int main(int argc, char **argv)
{
    char proce[1024] = {0};
    int verbose = 0;
    char info[256];

	if(argc == 2 && !strcmp(argv[1], "verbose")){
        verbose = 1;
    }
    
    check_proc_name(proce, sizeof(proce));
    signal_hanler_init(proce, &thread_running);
    log_init(CPU_0, 0, NULL, proce, 30, verbose);
    
    logi("==== process %s start ====\n", proce);
    sprintf(info, 
    "\n---------------------------------\n"
    "build: %s\n"
    "app:   %s\n"
    "algo:  %s\n"
    "---------------------------------\n",
    BUILD_TIME, 
    APP_VERSION, 
    check_algo_version());
    write_file_str(ver_path, info);
    logi("%s\n", info);

#if ENABLE_ALGO
    int algo_ret = 0;
    algo_ret = algo_init();
    if(algo_ret != 0){
        goto err_algo;
    }
#endif
    if(audio_start() < 0){
        goto err_audio_start;
    }

    int ad_reset = 0;
    unsigned char state = 0;
    init_led_run();
    while(thread_running){
		state = !state;
        led_run_state(state);
        sleep(1);
        if(!ad_reset && check_ad_start()){
            init_ad_reset();
            ad_reset = 1;
        }
    }

    if(ad_reset){
        exit_ad_reset();
    }
	exit_led_run();

    audio_stop();
err_audio_start:
#if ENABLE_ALGO
err_algo:
    algo_close(algo_ret);
#endif
    logi("==== process %s stop ====\n", proce);
    signal_hanler_exit();
    log_exit();

    return 0;
}