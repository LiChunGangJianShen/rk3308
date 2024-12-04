#include "rkgpio.h"
#include "thread.h"
#include "log.h"
#include <sys/prctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static pthread_state_t led_ctrl_task_state;

static void init_led_r(void)
{
    rk_set_gpio_export(1, 'A', 7);
    rk_set_gpio_direction_out(1, 'A', 7);
    rk_set_gpio_value(1, 'A', 7, 0);
}

static void exit_led_r(void)
{
    rk_set_gpio_value(1, 'A', 7, 0);
    rk_set_gpio_unexport(1, 'A', 7);
}

static void led_r_state(int state)
{
    if(state)
        rk_set_gpio_value(1, 'A', 7, 1);
    else
        rk_set_gpio_value(1, 'A', 7, 0);
}

static int led_ctrl_task(void *arg)
{
    prctl(PR_SET_NAME, "led_ctrl_task");
    logi("led ctrl task start\n");

    int state = 0;
    init_led_r();

    while (led_ctrl_task_state.running)
    {
        state = !state;
        led_r_state(state);
        sleep(1);
    }
    
    exit_led_r();

    logi("led ctrl task stop\n");
    return 0;
}

int led_ctrl_task_init(int cpu)
{
    int ret = 0;
    int high_priority = FALSE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&led_ctrl_task_state, 0, sizeof(led_ctrl_task_state));
    ret  = create_thread("led_ctrl_task", cpu, high_priority, led_ctrl_task, &led_ctrl_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "led_ctrl_task");
    }

    return ret;
}

void led_ctrl_task_exit(void)
{
    destroy_thread(&led_ctrl_task_state);
}