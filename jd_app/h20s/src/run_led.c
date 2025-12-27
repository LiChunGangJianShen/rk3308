#include "rkgpio.h"

void init_led_run(void)
{
    rk_set_gpio_export(0, 'C', 4);
    rk_set_gpio_direction_out(0, 'C', 4);
    rk_set_gpio_value(0, 'C', 4, 0);
}

void exit_led_run(void)
{
    rk_set_gpio_value(0, 'C', 4, 0);
    rk_set_gpio_unexport(0, 'C', 4);
}

void led_run_state(const unsigned char state)
{
    rk_set_gpio_value(0, 'C', 4, state);
}