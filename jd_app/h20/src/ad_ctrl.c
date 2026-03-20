#include "rkgpio.h"
#include <stdatomic.h>

atomic_int can_be_start = 0;

int check_ad_start() { return atomic_load(&can_be_start); }
void ad_can_be_to_start() { atomic_store(&can_be_start, 1); }

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