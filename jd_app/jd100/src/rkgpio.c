#include <stdio.h>
#include <string.h>
#include "log.h"

void rk_set_gpio_export(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num)
{
	FILE *fp = NULL;
	char file_name[128] = {0};
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/export");
	fp = fopen(file_name, "w");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return ;
	}
	fprintf(fp, "%d", gpio_num);
	fclose(fp);
}

void rk_set_gpio_unexport(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num)
{
	FILE *fp = NULL;
	char file_name[128] = {0};
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/unexport");
	fp = fopen(file_name, "w");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return ;
	}
	fprintf(fp, "%d", gpio_num);
	fclose(fp);
}

int rk_set_gpio_direction_out(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num)
{
	FILE *fp = NULL;
	char file_name[128];
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
	fp = fopen(file_name, "rb+");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return -1;
	}
	fprintf(fp, "out");
	fclose(fp);

	return 0;
}

int rk_set_gpio_direction_in(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num)
{
	FILE *fp = NULL;
	char file_name[128];
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
	fp = fopen(file_name, "rb+");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return -1;
	}
	fprintf(fp, "in");
	fclose(fp);

	return 0;
}

void rk_set_gpio_value(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num, unsigned int value)
{
	FILE *fp = NULL;
	char file_name[128] = {0};
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
	fp = fopen(file_name, "rb+");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return ;
	}
	fprintf(fp, "%d", value);
	fclose(fp);
}

int rk_get_gpio_value(unsigned int gpio_chip_num, unsigned char gpio_group_num, unsigned int gpio_offset_num)
{
	FILE *fp = NULL;
	char file_name[128];
	char buff[10];
	unsigned int gpio_num;

	gpio_num = gpio_chip_num * 32 + (gpio_group_num - 'A') * 8 + gpio_offset_num;
	sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
	fp = fopen(file_name, "rb+");
	if(fp == NULL){
		log_err("Cannot open %s", file_name);
		return -1;
	}

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(char), sizeof(buff) - 1, fp);
	fclose(fp);

	return (buff[0] - 48);
}
