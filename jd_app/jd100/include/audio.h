#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

int audio_start(int rate, int format, int algo_period_length);
char *algo_version(void);

#ifdef __cplusplus
}
#endif

#endif