#ifndef RK3308_SOC_H
#define RK3308_SOC_H

#ifdef __cplusplus
extern "C" {
#endif

#define RK3308G "RK3308G"
#define RK3308H "RK3308H"
#define RK3308HS "RK3308HS"
#define RK3308B "RK3308B"
#define RK3308BS "RK3308BS"

typedef enum{
    soc_rk3308g=0,
    soc_rk3308b,
    soc_rk3308bs,
    soc_rk3308h,
    soc_rk3308hs,
    soc_max,
}rk3308_soc_t;

rk3308_soc_t check_soc_type(void);
void cpu_type(void);

#ifdef __cplusplus
}
#endif

#endif