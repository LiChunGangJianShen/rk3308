#ifndef API_H
#define API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

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
