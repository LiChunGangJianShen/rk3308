#include "log.h"
#include "API.h"
#include "audio.h"

typedef char *(*ptr_ver)();
typedef bool (*ptr_init1)(short*, int);
typedef bool (*ptr_init)();
typedef void (*ptr_close)();
typedef int (*ptr_process1)(const short*, const short*, short*, const int);
typedef int (*ptr_process2)(const short*, const short*, const int);
typedef int (*ptr_process3)(const short*, const short*, const int);
typedef bool (*ptr_eq_setup)(float*);

ptr_ver algo_version = JD_M800V3_GetVersion;
ptr_init1 algo_init1 = JD_M800V3_Init1;
ptr_init algo_init2 = JD_M800V3_Init2;
ptr_init algo_init3 = JD_M800V3_Init3;
ptr_close algo_close1 = JD_M800V3_Delete1;
ptr_close algo_close2 = JD_M800V3_Delete2;
ptr_close algo_close3 = JD_M800V3_Delete3;
ptr_process1 algo_process1 = JD_M800V3_Process1;
ptr_process2 algo_process2 = JD_M800V3_Process2;
ptr_process3 algo_process3 = JD_M800V3_Process3;
ptr_eq_setup algo_set_eq = JD_M800V3_SetEQ;

enum
{
    algo_err_1 = -1,
    algo_err_2 = -2,
    algo_err_3 = -3,
    algo_err_max = 0,
};

int algo_init()
{
    if(algo_init1(NULL, 0) == false){
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

void _algo_process1(const short* mic_data, short* out_data, short* ref_data)
{
    algo_process1(mic_data, ref_data, out_data, ALGO_PERIOD_SIZE);
}

void _algo_process2(const short* mic_data, short* ref_data)
{
    algo_process2(mic_data, ref_data, ALGO_PERIOD_SIZE);
}

void _algo_process3(const short* mic_data, short* ref_data)
{
    algo_process3(mic_data, ref_data, ALGO_PERIOD_SIZE);
}

void _algo_set_eq(float *eq_val) 
{ 
    // algo_set_eq(eq_val); 
}