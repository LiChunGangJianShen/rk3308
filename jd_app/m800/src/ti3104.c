#include "comm.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
typedef struct{
    char name[256];
    int val;
}ti3104_ctrl_t;

static ti3104_ctrl_t ctrl[] = {
    {"De-emphasis Switch", 0},
    {"ADC HPF Cut-off", 1},//高通滤波开启，0.0045xFs 可以滤掉直流信号，这个配置还能保留大多数低频信号
    {"PGA Capture Switch", 1},

    {"Left PGA Capture Volume", 0},
    {"Right PGA Capture Volume", 0},

    {"Left PGA Mixer Mic2L Switch", 2},//0
    {"Left PGA Mixer Mic2R Switch", 0xf},
    {"Right PGA Mixer Mic2L Switch", 0xf},
    {"Right PGA Mixer Mic2R Switch", 0xf},//0

    {"Left Line1L Mux", 0},
    {"Left PGA Mixer Line1L Switch", 0xf},
    {"Left PGA Mixer Line1R Switch", 0xf},
    {"Right Line1R Mux", 0},
    {"Right PGA Mixer Line1L Switch", 0xf},
    {"Right PGA Mixer Line1R Switch", 1},//0

    {"Left HPCOM Mux", 0},
    {"Right HPCOM Mux", 0},
    {"Left DAC Mux", 0},
    {"Right DAC Mux", 0},
    {"PCM Playback Volume", 110},//127

    {"Left HP DAC Playback Volume", 118},
    {"Left HPCOM DAC Playback Volume", 118},
    {"Left HP Playback Volume", 0},
    {"Left HPCOM Playback Volume", 0},

    {"Right HP DAC Playback Volume", 118},
    {"Right HPCOM DAC Playback Volume", 118},
    {"Right HP Playback Volume", 0},
    {"Right HPCOM Playback Volume", 0},

    {"Left HP Mixer DACL1 Switch", 1},
    {"Right HP Mixer DACR1 Switch", 1},
    {"HP Playback Switch", 1},
    {"Left HPCOM Mixer DACL1 Switch", 1},
    {"Right HPCOM Mixer DACR1 Switch", 1},
    {"HPCOM Playback Switch", 1}
};

int ti3104_ctrl(const char *card)
{
    int val = 0;
    int control_num_max = ARRAY_SIZE(ctrl);

    if(!card){
        return -1;
    }

    for (int i = 0; i <control_num_max ; i++){
        alsa_cget(card, ctrl[i].name, &val);
        if(val != ctrl[i].val){
            val = ctrl[i].val;
            alsa_cset(card, ctrl[i].name, val);
        }
        delay_ms(1);
    }

    return 0;
}