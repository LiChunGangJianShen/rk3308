#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "rk3308_soc.h"

rk3308_soc_t check_soc_type(void)
{
    rk3308_soc_t soc_type;
    int generation;
    int type;
    char buf[1024];

    FILE *fp1 = NULL, *fp2 = NULL;
    fp1 = popen("hexdump -C /sys/bus/nvmem/devices/rockchip-otp0/nvmem | awk 'NR==1{print $8}'", "r");
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), fp1);
    type = strtol(buf, NULL, 16);
    pclose(fp1);
    memset(buf, 0, sizeof(buf));
    fp2 = popen("hexdump -C /sys/bus/nvmem/devices/rockchip-otp0/nvmem | awk 'NR==2{print $14}'", "r");
    fread(buf, 1, sizeof(buf), fp2);
    generation = strtol(buf, NULL, 16);
    pclose(fp2);

    type &= 0x1f;
    generation = (generation >> 6) & 0x3;
    // log_dbg("generation=%d, type=%d", generation, type);

    if(type == 0x02){
        if(generation == 0x01)
            soc_type = soc_rk3308b;
        else if(generation == 0x02)
            soc_type = soc_rk3308bs;
        else{
            log_dbg("Error soc generation");
            soc_type = soc_max;
        }
    }
    else if(type == 0x07){
        soc_type = soc_rk3308g;
    }
    else if(type == 0x08){
        if(generation == 0x01)
            soc_type = soc_rk3308h;
        else if(generation == 0x02)
            soc_type = soc_rk3308hs;
        else{
            log_dbg("Error soc generation");
            soc_type = soc_max;
        }
    }
    else{
        log_dbg("Error soc type");
        soc_type = soc_max;
    }

    return soc_type;
}

void cpu_type(void)
{
    rk3308_soc_t soc_type = check_soc_type();
    switch(soc_type){
        case soc_rk3308g:
            log_dbg("cpu-type: %s", RK3308G);
            break;
        case soc_rk3308h:
            log_dbg("cpu-type: %s", RK3308H);
            break;
        case soc_rk3308hs:
            log_dbg("cpu-type: %s", RK3308HS);
            break;
        case soc_rk3308b:
            log_dbg("cpu-type: %s", RK3308B);
            break;
        case soc_rk3308bs:
            log_dbg("cpu-type: %s", RK3308BS);
            break;
        case soc_max:
        default:
            log_dbg("unknow soc-type");
            break;
    }
}