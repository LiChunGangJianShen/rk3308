#ifndef RK3308_SOC_H
#define RK3308_SOC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    soc_rk3308g=0,
    soc_rk3308b,
    soc_rk3308bs,
    soc_rk3308h,
    soc_rk3308hs,
    soc_max,
}rk3308_soc_t;

rk3308_soc_t check_soc_type(void);

#ifdef __cplusplus
}
#endif

#endif