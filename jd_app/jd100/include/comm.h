#ifndef _COMM_H_
#define _COMM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

unsigned long get_sys_ms(void);
int get_executable_path( char* processdir,char* processname, int len);
int generate_1khz_sine_wave(int lenght, int sample_rate, void *sine_wave_buff);
unsigned long check_time_increment_ms(struct timeval tva, struct timeval tvb);

#ifdef __cplusplus
}
#endif

#endif