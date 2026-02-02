#include "rkgpio.h"

void run_led_init()
{
    rk_set_gpio_export(1, 'A', 7);
    rk_set_gpio_direction_out(1, 'A', 7);
    rk_set_gpio_export(1, 'B', 0);
    rk_set_gpio_direction_out(1, 'B', 0);
    rk_set_gpio_value(1, 'B', 0, 1);
}

void run_led_state(const unsigned char state)
{
    rk_set_gpio_value(1, 'A', 7, state);
}

void run_led_uninit()
{
    rk_set_gpio_value(1, 'A', 7, 0);
    rk_set_gpio_unexport(1, 'A', 7);
    rk_set_gpio_value(1, 'B', 0, 0);
    rk_set_gpio_unexport(1, 'B', 0);
}