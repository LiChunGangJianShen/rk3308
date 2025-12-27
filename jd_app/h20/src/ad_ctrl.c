#include "rkgpio.h"

void init_ad_reset(void)
{
    rk_set_gpio_export(0, 'A', 3);
    rk_set_gpio_direction_out(0, 'A', 3);
    rk_set_gpio_value(0, 'A', 3, 1);
}

void exit_ad_reset(void)
{
    rk_set_gpio_value(0, 'A', 3, 0);
    rk_set_gpio_unexport(0, 'A', 3);
}