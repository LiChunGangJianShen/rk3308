#include "rkgpio.h"

void enable_aec_out() 
{ 
    rk_set_gpio_export(0, 'A', 0);
    rk_set_gpio_direction_out(0, 'A', 0);
    rk_set_gpio_value(0, 'A', 0, 1);
}

void disable_aec_out()
{
    rk_set_gpio_value(0, 'A', 0, 0);
    rk_set_gpio_unexport(0, 'A', 0);
}

void enable_spk_out() 
{ 
    rk_set_gpio_export(0, 'A', 1);
    rk_set_gpio_direction_out(0, 'A', 1);
    rk_set_gpio_value(0, 'A', 1, 1);
}

void disable_spk_out()
{
    rk_set_gpio_value(0, 'A', 1, 0);
    rk_set_gpio_unexport(0, 'A', 1);
}