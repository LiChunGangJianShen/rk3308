#ifndef OUT_CTRL_H
#define OUT_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void enable_aec_out();
void disable_aec_out();
void enable_spk_out();
void disable_spk_out();

#ifdef __cplusplus
}
#endif

#endif // OUT_CTRL_H