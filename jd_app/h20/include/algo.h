#ifndef ALGO_H
#define ALGO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "audio.h"

int algo_init();
void algo_close(int err);
char *check_algo_version();
bool pinknoise_switch(int onoff, float gain);
bool feed_back_switch(int onoff);
#if ENABLE_ALGO_AI
bool ai_switch(int onoff);
#endif
bool feed_back_mute(int mute);
bool feed_back_set_micgain(float gain);
bool feed_back_set_eq(float *val);
void _algo_process1(const audio_fmt_t *mic_data1, const audio_fmt_t *mic_data2, audio_fmt_t *out_data1, audio_fmt_t *out_data2, audio_fmt_t *ref_data1, audio_fmt_t *ref_data2);
void _algo_process2(const audio_fmt_t *mic_data1, const audio_fmt_t *mic_data2, audio_fmt_t *ref_data1, audio_fmt_t *ref_data2);
void _algo_process3(const audio_fmt_t *mic_data1, const audio_fmt_t *mic_data2, audio_fmt_t *ref_data1, audio_fmt_t *ref_data2);

#ifdef __cplusplus
}
#endif

#endif // ALGO_H