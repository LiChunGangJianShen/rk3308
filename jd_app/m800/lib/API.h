#pragma once


extern "C" bool JD_M800V3_Init1(short* addr_play = 0, int num_play = 0);
extern "C" void JD_M800V3_Delete1();
extern "C" int JD_M800V3_Process1(const short* mic_data, const short* ref_data, short* out_data, const int nums);

extern "C" bool JD_M800V3_Init2();
extern "C" void JD_M800V3_Delete2();
extern "C" int JD_M800V3_Process2(const short* mic_data, const short* ref_data, const int nums);

extern "C" bool JD_M800V3_Init3();
extern "C" void JD_M800V3_Delete3();
extern "C" int JD_M800V3_Process3(const short* mic_data, const short* ref_data, const int nums);

extern "C" char* JD_M800V3_GetVersion();



extern "C" bool JD_M800V3_SetPickupMode(int work_mode);

extern "C" bool JD_M800V3_SetEQ(float* EQ_dB);
extern "C" void JD_M800V3_SetParameters(const short* Param);

// 设置麦克增益，取值范围[-30.0, 10.0] dB, 默认为0dB
extern "C" bool JD_M800V3_SetMicGain(const float gain_dB = 0.0f);
