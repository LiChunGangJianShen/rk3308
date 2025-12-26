#ifndef RKGPIO_H
#define RKGPIO_H

#ifdef __cplusplus
extern "C" {
#endif

void rk_set_gpio_export(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num);
void rk_set_gpio_unexport(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num);
int rk_set_gpio_direction_out(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num);
int rk_set_gpio_direction_in(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num);
void rk_set_gpio_value(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num, unsigned int value);
int rk_get_gpio_value(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num);

#ifdef __cplusplus
}
#endif

#endif