#ifndef _LED_CTRL_H_
#define _LED_CTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

int led_ctrl_task_init(int cpu);
void led_ctrl_task_exit(void);

#ifdef __cplusplus
}
#endif

#endif