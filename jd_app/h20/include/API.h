#ifndef API_H
#define API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// AI降噪开关, 0关闭 1开启(默认 0关闭)
bool JDZH_FeedbackDestroy_AIOnOff(int ai_flag);

// 白噪声输出模式
// pinknoise_flag = 0: 关闭白噪声模式; 1: 输出白噪声
// noise_gain 白噪声幅度增益, 取值范围(0.1, 10.0)，默认1.0
bool JDZH_FeedbackDestroy_PinkNoiseOnOff(int pinknoise_flag, float noise_gain);

// 反馈抑制 开关, 0关闭 1开启(默认 1)
bool JDZH_FeedbackDestroy_FeedbackOnOff(int feedback_flag);
// 静音 开关, 0静音关闭 1静音启动(默认 0)
bool JDZH_FeedbackDestroy_MuteOnOff(int mute_flag);
// EQ 16段 
bool JDZH_FeedbackDestroy_SetEQ(float* EQ_dB);
// 设置麦克输入增益, -30.0 < gain_dB < 30.0 (默认 0.0f dB)
bool JDZH_FeedbackDestroy_SetInputMicGain(float gain_dB);

bool JDZH_FeedbackDestroy_Init1();
bool JDZH_FeedbackDestroy_Init2();
bool JDZH_FeedbackDestroy_Init3();

void JDZH_FeedbackDestroy_Close1();
void JDZH_FeedbackDestroy_Close2();
void JDZH_FeedbackDestroy_Close3();

void JDZH_FeedbackDestroy_Process1(const short* mic_data1, const short* mic_data2, short* out_data1, short* out_data2, short* ref_data1, short* ref_data2);
void JDZH_FeedbackDestroy_Process2(const short* mic_data1, const short* mic_data2, short* ref_data1, short* ref_data2);
void JDZH_FeedbackDestroy_Process3(const short* mic_data1, const short* mic_data2, short* ref_data1, short* ref_data2);

// 获取版本号
char* JDZH_FeedbackDestroy_GetVersion();

#ifdef __cplusplus
}
#endif

#endif
