#include "comm.h"

// chip+module+reg
enum {
    es8327_adc_2c=0,
    es8327_dac_50,
    es8327_max,
};

typedef struct{
    int val;
    char name[256];
}es8327_ctrl_t;

static es8327_ctrl_t ctrl[es8327_max] = {
    {191, "ADC Capture Volume"},
    {185, "DAC Playback Volume"},//0dB输出时，当声音较大的时候会有滋啦滋啦声，改为-3dB
};

int es8327_ctrl(const char *card)
{
    int val = 0;

    if(!card){
        return -1;
    }

    for (int i = 0; i < es8327_max; i++){
        alsa_cget(card, ctrl[i].name, &val);
        if(val != ctrl[i].val){
            val = ctrl[i].val;
            alsa_cset(card, ctrl[i].name, val);
        }
    }

    return 0;
}