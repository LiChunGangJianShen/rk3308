#ifndef ALGO_H
#define ALGO_H

#ifdef __cplusplus
extern "C" {
#endif

int algo_init();
void algo_close(int err);
char *check_algo_version();
bool pinknoise_switch(int onoff, float gain);
bool feed_back_switch(int onoff);
bool ai_switch(int onoff);
bool feed_back_mute(int mute);
bool feed_back_set_micgain(float gain);
bool feed_back_set_eq(float *val);
void _algo_process1(const short *mic_data1, const short *mic_data2, short *out_data1, short *out_data2, short *ref_data1, short *ref_data2);
void _algo_process2(const short *mic_data1, const short *mic_data2, short *ref_data1, short *ref_data2);
void _algo_process3(const short *mic_data1, const short *mic_data2, short *ref_data1, short *ref_data2);

#ifdef __cplusplus
}
#endif

#endif // ALGO_H