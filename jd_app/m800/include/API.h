#ifndef API_H
#define API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool JD_M800V3_Init1(short* addr_play, int num_play);
bool JD_M800V3_Init2();
bool JD_M800V3_Init3();

void JD_M800V3_Delete1();
void JD_M800V3_Delete2();
void JD_M800V3_Delete3();

int JD_M800V3_Process1(const short* mic_data, const short* ref_data, short* out_data, const int nums);
int JD_M800V3_Process2(const short* mic_data, const short* ref_data, const int nums);
int JD_M800V3_Process3(const short* mic_data, const short* ref_data, const int nums);

char* JD_M800V3_GetVersion();
bool JD_M800V3_SetPickupMode(int work_mode);
bool JD_M800V3_SetEQ(float* EQ_dB);
void JD_M800V3_SetParameters(const short* Param);
bool JD_M800V3_SetMicGain(const float gain_dB);

bool JD_MicArray_Init1();
bool JD_MicArray_Init2();
bool JD_MicArray_Init3();

void JD_MicArray_Close1();
void JD_MicArray_Close2();
void JD_MicArray_Close3();

void JD_MicArray_Process1(const short* mic_data, short* out_data, short* ref_data);
void JD_MicArray_Process2(const short* mic_data, short* ref_data);
void JD_MicArray_Process3(const short* mic_data, short* ref_data);

char* JD_MicArray_GetVersion();

#ifdef __cplusplus
}
#endif
#endif
