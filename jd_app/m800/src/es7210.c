#include "comm.h"

#define ES7210_CHANNELS_MAX 8

// chip+module+reg
enum {
    es7210_mic1gain_43_1=0,
    es7210_mic2gain_44_1,
    es7210_mic3gain_45_1,
    es7210_mic4gain_46_1,
#if ES7210_CHANNELS_MAX > 4
    es7210_mic1gain_43_2,
    es7210_mic2gain_44_2,
    es7210_mic3gain_45_2,
    es7210_mic4gain_46_2,
#endif
    es7210_max,
};

typedef struct{
    int val;
    char name[256];
}es7210_ctrl_t;

static es7210_ctrl_t ctrl[es7210_max] = {
    {7, "PGA1_setting"},
    {7, "PGA2_setting"},
    {7, "PGA3_setting"},
    {7, "PGA4_setting"},
#if ES7210_CHANNELS_MAX > 4
    {7, "PGA5_setting"},
    {7, "PGA6_setting"},
    {7, "PGA7_setting"},
    {7, "PGA8_setting"},
#endif
};

int es7210_ctrl(const char *card)
{
    int val = 0;

    if(!card){
        return -1;
    }

    for (int i = 0; i < es7210_max; i++){
        alsa_cget(card, ctrl[i].name, &val);
        if(val != ctrl[i].val){
            val = ctrl[i].val;
            alsa_cset(card, ctrl[i].name, val);
        }
    }

    return 0;
}