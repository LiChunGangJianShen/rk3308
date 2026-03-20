#ifndef AD_CTRL_H
#define AD_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void init_ad_reset(void);
void exit_ad_reset(void);
int check_ad_start();
void ad_can_be_to_start();

#ifdef __cplusplus
}
#endif

#endif // AD_CTRL_H