#ifndef RUN_LED_H
#define RUN_LED_H

#ifdef __cplusplus
extern "C" {
#endif

void init_led_run(void);
void exit_led_run(void);
void led_run_state(const unsigned char state);

#ifdef __cplusplus
}
#endif

#endif // RUN_LED_H