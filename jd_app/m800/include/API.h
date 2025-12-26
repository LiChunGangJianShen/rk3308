#ifndef API_H
#define API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

char* JD901_GetVersion();

// void JD_M800V3_SetInitDecayGainTime(float gain_decay_dB = -6.0f, float time_dacay = 30.0f);
// void JD_M800V3_SetAGCDRC(float gain_dB = -2.0);	
// void JD_M800V3_VoiceWallControl(int vw_on = 1); 

// bool JD_M800V3_SetEQ(float* EQ_dB);

// void JD_M800V3_SetParameters(const short* Param);

bool JD_M800V3_Init1(short* addr_play, int num_play);
void JD_M800V3_Delete1();
int JD_M800V3_Process1(const short* mic_data, const short* ref_data, short* out_data, const int nums);

bool JD_M800V3_Init2();
void JD_M800V3_Delete2();
int JD_M800V3_Process2(const short* mic_data, const short* ref_data, const int nums);

bool JD_M800V3_Init3();
void JD_M800V3_Delete3();
int JD_M800V3_Process3(const short* mic_data, const short* ref_data, const int nums);

char* JD_M800V3_GetVersion();

#ifdef __cplusplus
}
#endif
#endif
