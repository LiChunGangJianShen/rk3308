#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <sys/resource.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include "log.h"
#include "comm.h"

#ifdef __cplusplus
extern "C" {
#endif

char *check_proc_name(char *buffer, size_t buffer_size) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return NULL;
    }
    exe_path[len] = '\0';

    char path_copy[PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", exe_path);

    char *name = basename(path_copy);

    char *deleted_suffix = strstr(name, " (deleted)");
    if (deleted_suffix) {
        *deleted_suffix = '\0';
    }

    strncpy(buffer, name, buffer_size);
    buffer[buffer_size - 1] = '\0';
    return buffer;
}

void write_file_str(const char *file, const char *str)
{
	FILE *fp;
	
	if(!file || !str) {
		return;
	}

	fp = fopen(file, "w");
	if(fp) {
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	} else {
		loge("write file=%s fail!!!\n", file);
	}
}

unsigned long check_time_increment_ms(struct timeval tvlast, struct timeval tvcur)
{
    if(tvcur.tv_sec < tvlast.tv_sec || 
        ((tvcur.tv_sec == tvlast.tv_sec) && (tvcur.tv_usec < tvlast.tv_usec))){
        return -1;
    }

    long long sec_diff = (long long)(tvcur.tv_sec - tvlast.tv_sec);
    long long usec_diff = (long long)(tvcur.tv_usec - tvlast.tv_usec);
    long long total_us = sec_diff * 1000000LL + usec_diff;

    return (unsigned long)(total_us / 1000);
}

unsigned long check_time_increment_s(struct timeval tvlast, struct timeval tvcur)
{
    if(tvcur.tv_sec < tvlast.tv_sec || 
        ((tvcur.tv_sec == tvlast.tv_sec) && (tvcur.tv_usec < tvlast.tv_usec))){
        return -1;
    }

    long long sec_diff = (long long)(tvcur.tv_sec - tvlast.tv_sec);
    long long usec_diff = (long long)(tvcur.tv_usec - tvlast.tv_usec);
    long long total_us = sec_diff * 1000000LL + usec_diff;

    return (unsigned long)(total_us / 1000000);
}

double check_time_increment_ms_f(struct timeval tvlast, struct timeval tvcur)
{
    if(tvcur.tv_sec < tvlast.tv_sec || 
        ((tvcur.tv_sec == tvlast.tv_sec) && (tvcur.tv_usec < tvlast.tv_usec))){
        return -1;
    }

    long long sec_diff = (long long)(tvcur.tv_sec - tvlast.tv_sec);
    long long usec_diff = (long long)(tvcur.tv_usec - tvlast.tv_usec);
    long long total_us = sec_diff * 1000000LL + usec_diff;

    return (double)(total_us / 1000.0);
}

int set_stack_size(size_t new_size) 
{
    struct rlimit rl;
    
    // ��ȡ��ǰջ����
    if (getrlimit(RLIMIT_STACK, &rl) != 0) {
        perror("getrlimit");
        return -1;
    }
    
    logi("Current stack limits: soft=%lu, hard=%lu\n", 
           (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);
    
    // �����µ������ƣ����ܳ���Ӳ���ƣ�
    if (new_size > rl.rlim_max) {
        logi("Warning: Requested size exceeds hard limit\n");
        new_size = rl.rlim_max;
    }
    
    rl.rlim_cur = new_size;
    
    // Ӧ��������
    if (setrlimit(RLIMIT_STACK, &rl) != 0) {
        perror("setrlimit\n");
        return -1;
    }
    
    logi("New stack size: %lu bytes\n", (unsigned long)new_size);
    return 0;
}

void delay_s(int s)
{
    if(s <= 0){
        return;
    }

    sleep(s);
}

void delay_ms(int ms)
{
    if(ms <= 0){
        return;
    }

    usleep(ms*1000);
}

void delay_us(int us)
{
    if(us <= 0){
        return;
    }

    usleep(us);
}

int alsa_cget(const char *card, const char *name, void *val)
{
    snd_ctl_t *handle;
    int err = 0;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;

    if(!card || !name || !val)
        return -1;

    snd_config_update_free_global();
    
    if((err = snd_ctl_open(&handle, card, SND_CTL_NONBLOCK)) < 0){
        loge("snd_ctl_open error(%s)\n", snd_strerror(err));
        return err;
    }

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_id_set_index(id, 0);

    snd_ctl_elem_value_set_id(control, id);

    err = snd_ctl_elem_read(handle, control);
    if(err >= 0){
        *(int *)val = snd_ctl_elem_value_get_integer(control, 0);
    }

    snd_ctl_close(handle);

    return err;
}

int alsa_cset(const char *card, const char *name, int val)
{
    snd_ctl_t *handle;
    int err = 0;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;

    if(!card || !name)
        return -1;

    snd_config_update_free_global();

    if((err = snd_ctl_open(&handle, card, SND_CTL_NONBLOCK)) < 0){
        loge("snd_ctl_open error(%s)\n", snd_strerror(err));
        return err;
    }

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_id_set_index(id, 0);

    snd_ctl_elem_value_set_id(control, id);
    snd_ctl_elem_value_set_integer(control, 0, val);

    err = snd_ctl_elem_write(handle, control);

    snd_ctl_close(handle);

    return err;
}

void generate_1khz_16bit_sine_wave(int lenght, int sample_rate, void *sine_wave_buff)
{
	if(lenght < 0 || sample_rate > 96000 || sample_rate < 8000){
		printf("invalid parameter\n");
		return;
	}

	int len_buff = sample_rate * lenght / 1000;
	short *buff = (short *)sine_wave_buff;
#define BASIC_AMPLITUDE		(16384)
#define PI		(3.1415926)
	int i;
	for(i = 0; i < len_buff; i++){
		buff[i] = BASIC_AMPLITUDE * sin(2 * PI * (i % (sample_rate / 1000)) / (sample_rate / 1000));
	}
}

void generate_1khz_24bit_sine_wave(int lenght, int sample_rate, void *sine_wave_buff)
{
	if(lenght < 0 || sample_rate > 96000 || sample_rate < 8000){
		printf("invalid parameter\n");
		return;
	}

	int len_buff = sample_rate * lenght / 1000;
	int *buff = (int *)sine_wave_buff;
    int basic_amplitude = 0x7FFFFFFF >> 8;
#define PI		(3.1415926)
	int i;
	for(i = 0; i < len_buff; i++){ 
		buff[i] = basic_amplitude * sin(2 * PI * (i % (sample_rate / 1000)) / (sample_rate / 1000));
	}
}

#ifdef __cplusplus
}
#endif