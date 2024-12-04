#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include "log.h"

int get_executable_path( char* processdir,char* processname, int len)
{
    char* path_end;
    if(readlink("/proc/self/exe", processdir,len) <=0)
        return -1;
    path_end = strrchr(processdir,  '/');
    if(path_end == NULL)
        return -1;
    ++path_end;
    strcpy(processname, path_end);
    *path_end = '\0';
    return (size_t)(path_end - processdir);
}

unsigned long get_sys_ms(void)
{
    struct timeval tv;
    unsigned long t;

    gettimeofday(&tv, NULL);
    t = (unsigned long)tv.tv_sec * 1000 + (unsigned long)tv.tv_usec / 1000;

    return t;
}

unsigned long check_time_increment_ms(struct timeval tva, struct timeval tvb)
{
    return ((tvb.tv_sec-tva.tv_sec)*1000 + (tvb.tv_usec-tva.tv_usec)/1000);
}

int generate_1khz_sine_wave(int lenght, int sample_rate, void *sine_wave_buff)
{
	if(lenght < 0 || sample_rate > 96000 || sample_rate < 8000){
		printf("invalid parameter\n");
		return -1;
	}

	int len_buff = sample_rate * lenght / 1000;
	short *buff = (short *)sine_wave_buff;
#define BASIC_AMPLITUDE		2//(16384)
#define PI		(3.1415926)
	int i;
	for(i = 0; i < len_buff; i++){
		buff[i] = BASIC_AMPLITUDE * sin(2 * PI * (i % (sample_rate / 1000)) / (sample_rate / 1000));
	}

	return 0;
}