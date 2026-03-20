#include "log.h"
#include "API.h"

typedef bool (*ptr_noise)(int, float);
typedef bool (*ptr_feedback_switch)(int);
typedef bool (*ptr_ai_switch)(int);
typedef bool (*ptr_mute)(int);
typedef bool (*ptr_seteq)(float*);
typedef bool (*ptr_setmicgain)(float);
typedef char *(*ptr_ver)();
typedef bool (*ptr_init)();
typedef void (*ptr_close)();
typedef void (*ptr_process1)(const audio_fmt_t*, const audio_fmt_t*, audio_fmt_t*, audio_fmt_t*, audio_fmt_t*, audio_fmt_t*);
typedef void (*ptr_process2)(const audio_fmt_t*, const audio_fmt_t*, audio_fmt_t*, audio_fmt_t*);
typedef void (*ptr_process3)(const audio_fmt_t*, const audio_fmt_t*, audio_fmt_t*, audio_fmt_t*);

ptr_noise algo_pinknoise = JDZH_FeedbackDestroy_PinkNoiseOnOff;
ptr_feedback_switch algo_feedback_switch = JDZH_FeedbackDestroy_FeedbackOnOff;
ptr_ai_switch algo_ai_switch = JDZH_FeedbackDestroy_AIOnOff;
ptr_mute algo_mute = JDZH_FeedbackDestroy_MuteOnOff;
ptr_seteq algo_seteq = JDZH_FeedbackDestroy_SetEQ;
ptr_setmicgain algo_set_micgain = JDZH_FeedbackDestroy_SetInputMicGain;
ptr_ver algo_version = JDZH_FeedbackDestroy_GetVersion;
ptr_init algo_init1 = JDZH_FeedbackDestroy_Init1;
ptr_init algo_init2 = JDZH_FeedbackDestroy_Init2;
ptr_init algo_init3 = JDZH_FeedbackDestroy_Init3;
ptr_close algo_close1 = JDZH_FeedbackDestroy_Close1;
ptr_close algo_close2 = JDZH_FeedbackDestroy_Close2;
ptr_close algo_close3 = JDZH_FeedbackDestroy_Close3;
ptr_process1 algo_process1 = JDZH_FeedbackDestroy_Process1;
ptr_process2 algo_process2 = JDZH_FeedbackDestroy_Process2;
ptr_process3 algo_process3 = JDZH_FeedbackDestroy_Process3;

enum
{
    algo_err_1 = -1,
    algo_err_2 = -2,
    algo_err_3 = -3,
    algo_err_max = 0,
};

int algo_init()
{
    short addr_play = 0;
    int num_play = 0;
    if(algo_init1(&addr_play, num_play) == false){
        loge("---- algo 1 init failed----\n");
        return algo_err_1;
    }
    if(algo_init2() == false){
        loge("---- algo 2 init failed----\n");
        return algo_err_2;
    }
    if(algo_init3() == false){
        loge("---- algo 3 init failed----\n");
        return algo_err_3;
    }

    algo_ai_switch(1);
    logi("---- algo all init success ----\n");
    return algo_err_max;
}

void algo_close(int err)
{
    if(err == algo_err_1){
        return;
    }
    if(err == algo_err_2){
        algo_close1();
        return;
    }
    if(err == algo_err_3){
        algo_close1();
        algo_close2();
        return;
    }

    algo_close1();
    algo_close2();
    algo_close3();
}

char *check_algo_version()
{
    return algo_version();
}

void _algo_process1(
    const audio_fmt_t* mic_data1, 
    const audio_fmt_t* mic_data2, 
    audio_fmt_t* out_data1, 
    audio_fmt_t* out_data2, 
    audio_fmt_t* ref_data1, 
    audio_fmt_t* ref_data2)
{
    algo_process1(mic_data1, mic_data2, out_data1, out_data2, ref_data1, ref_data2);
}

void _algo_process2(
    const audio_fmt_t* mic_data1, 
    const audio_fmt_t* mic_data2, 
    audio_fmt_t* ref_data1, 
    audio_fmt_t* ref_data2)
{
    algo_process2(mic_data1, mic_data2, ref_data1, ref_data2);
}

void _algo_process3(
    const audio_fmt_t* mic_data1, 
    const audio_fmt_t* mic_data2, 
    audio_fmt_t* ref_data1, 
    audio_fmt_t* ref_data2)
{
    algo_process3(mic_data1, mic_data2, ref_data1, ref_data2);
}

bool pinknoise_switch(int onoff, float gain)
{ 
    return algo_pinknoise(onoff, gain); 
}

bool feed_back_switch(int onoff)
{ 
    return algo_feedback_switch(onoff); 
}

bool ai_switch(int onoff)
{ 
    return algo_ai_switch(onoff); 
}

bool feed_back_mute(int mute)
{ 
    return algo_mute(mute); 
}

bool feed_back_set_micgain(float gain) 
{ 
    return algo_set_micgain(gain); 
}

bool feed_back_set_eq(float *val) 
{ 
    return algo_seteq(val); 
}