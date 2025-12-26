#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#define EN_USB_HOTPLUG  1
#define EN_UAC  0
#define EN_ALGO 1

#define SAMPLE_RATE 16000
#define MIC_CHN     8
#define UAC_AI_CHN  2
#define UAC_AO_CHN  2
#define LINE_AI_CHN  2
#define LINE_AO_CHN  2
#define MIC_PERIOD_SIZE 16
#define UAC_PERIOD_SIZE 16
#define LINE_PERIOD_SIZE    16
#define ALGO_PERIOD_SIZE    48
#define MIC_CNT 7
#define REF_CNT 1
#define OUT_CNT 1

int audio_start();
void audio_stop();

#ifdef __cplusplus
}
#endif

#endif // AUDIO_H