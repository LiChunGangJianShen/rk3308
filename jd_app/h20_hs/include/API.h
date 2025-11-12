#ifndef API_H
#define API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>


// 反馈抑制 开关, 0关闭 1开启(默认 1)
bool JDZH_H20S_FeedbackOnOff(int feedback_flag);
// 静音 开关, 0静音关闭 1静音启动(默认 0)
bool JDZH_H20S_MuteOnOff(int mute_flag);
// EQ 16段 
bool JDZH_H20S_SetEQ(float* EQ_dB);
// 设置麦克输入增益, -30.0 < gain_dB < 30.0 (默认 0.0f dB)
bool JDZH_H20S_SetInputMicGain(float gain_dB);

bool JDZH_H20S_Init1();
bool JDZH_H20S_Init2();
bool JDZH_H20S_Init3();

void JDZH_H20S_Close1();
void JDZH_H20S_Close2();
void JDZH_H20S_Close3();

void JDZH_H20S_Process1(const short* mic_data1, const short* mic_data2, short* out_data1, short* out_data2, short* ref_data1, short* ref_data2);
void JDZH_H20S_Process2(const short* mic_data1, const short* mic_data2, short* ref_data1, short* ref_data2);
void JDZH_H20S_Process3(const short* mic_data1, const short* mic_data2, short* ref_data1, short* ref_data2);

char* JDZH_H20S_GetVersion();


#ifdef __cplusplus
}
#endif

#endif
