#ifndef RUN_LED_H
#define RUN_LED_H

#ifdef __cplusplus
extern "C" {
#endif

void run_led_init();
void run_led_state(const unsigned char state);
void run_led_uninit();

#ifdef __cplusplus
}
#endif

#endif // RUN_LED_H