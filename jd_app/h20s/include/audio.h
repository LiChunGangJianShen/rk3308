#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_ALGO     1
#define TWO_OUT_DATA    0
#define EN_REC_WAV_FILE	0
#define SAMPLE_RATE     22050
#define PERIOD_SIZE     48
#define PERIODS         2
#define ALG_FRAMES      48
#define CAPTURE_CHN     2
#define PLAYBACK_CHN    2
#define REC_CHN         4

int audio_start();
void audio_stop();

#if defined(RK3308_CODEC_EN)
void init_rk3308_codec(char *card);
#endif

#ifdef __cplusplus
}
#endif

#endif
