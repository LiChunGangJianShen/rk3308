#ifndef ALGO_H
#define ALGO_H

#ifdef __cplusplus
extern "C" {
#endif

int algo_init();
void algo_close(int err);
char *check_algo_version();
void _algo_process1(const short *mic_data, short *out_data, short *ref_data);
void _algo_process2(const short *mic_data, short *ref_data);
void _algo_process3(const short *mic_data, short *ref_data);
void _algo_set_eq(float *eq_val);

#ifdef __cplusplus
}
#endif

#endif // ALGO_H