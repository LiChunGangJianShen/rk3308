#ifndef _COMM_H_
#define _COMM_H_

#include <stddef.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CPU_0   (0)
#define CPU_1   (1)
#define CPU_2   (2)
#define CPU_3   (3)
#define safe_free(p) do { free(p); (p) = NULL; } while(0)

char *check_proc_name(char *buffer, size_t buffer_size);
void write_file_str(const char *file, const char *str);
unsigned long check_time_increment_ms(struct timeval tvlast, struct timeval tvcur);
unsigned long check_time_increment_s(struct timeval tvlast, struct timeval tvcur);
double check_time_increment_ms_f(struct timeval tvlast, struct timeval tvcur);
int set_stack_size(size_t stack_size);

void delay_s(int s);
void delay_ms(int ms);
void delay_us(int us);

int alsa_cget(const char *card, const char *name, void *val);
int alsa_cset(const char *card, const char *name, int val);

void generate_1khz_16bit_sine_wave(int lenght, int sample_rate, void *sine_wave_buff);
void generate_1khz_24bit_sine_wave(int lenght, int sample_rate, void *sine_wave_buff);

#ifdef __cplusplus
}
#endif

#endif