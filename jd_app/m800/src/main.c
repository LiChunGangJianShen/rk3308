#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "signal_handle.h"
#include "comm.h"
#include "log.h"
#include "build_time.h"
#include "app_version.h"
#include "algo.h"
#include "audio.h"
#include "run_led.h"

// static const size_t stack_size = 100 * 1024 * 1024;
static const char *ver_path = "/tmp/version";
static int g_running = 1;

int main(int argc, char **argv)
{
    char proce[1024] = {0};
    int verbose = 0;
    int led_sta = 1;

    if(argc == 2 && !strcmp(argv[1], "verbose")){
        verbose = 1;
    }
    // set_stack_size(stack_size);

    check_proc_name(proce, sizeof(proce));
    signal_hanler_init(proce, &g_running);
    log_init(CPU0, 0, NULL, proce, 30, verbose);

    logi("==== process %s start ====\n", proce);

    char info[384] = {0};
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
#if EN_ALGO
    int algo_ret = algo_init();
    if(algo_ret != 0){
        goto algo_init_failed;
    }
#endif
    if(audio_start() < 0){
        goto __audio_start_err;
    }

    run_led_init();
    while(g_running){
        run_led_state(led_sta);
        led_sta = !led_sta;
        sleep(1);
    }
    run_led_uninit();

__audio_start_err:
    audio_stop();
#if EN_ALGO
    algo_close(algo_ret);
algo_init_failed:
#endif
    logi("==== process %s stop ====\n", proce);
    signal_hanler_exit();
    log_exit();

    return 0;
}