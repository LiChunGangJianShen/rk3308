#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define USING_48K_64FRAME   0
#define	ENABLE_ALGO_AI	1
#define AI_SW_DEFAULT   1
#define EN_32BIT        0
#define ENABLE_ALGO     1
#define TWO_OUT_DATA    0
#if USING_48K_64FRAME
#define SAMPLE_RATE     48000
#define PERIOD_SIZE     24
#define ALG_FRAMES      64
#else
#define SAMPLE_RATE     22050
#define PERIOD_SIZE     16
#define ALG_FRAMES      48
#endif
#define PERIODS         2
#define CAPTURE_CHN     2
#define PLAYBACK_CHN    2
#define REC_CHN         4
#if EN_32BIT
#define DATA_BIT        32
#else
#define DATA_BIT        16
#endif

#if EN_32BIT
typedef int audio_fmt_t;
#else
typedef short audio_fmt_t;
#endif

int audio_start();
void audio_stop();

#ifdef __cplusplus
}
#endif

#endif
