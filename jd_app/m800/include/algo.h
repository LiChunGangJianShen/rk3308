#ifndef ALGO_H
#define ALGO_H

#ifdef __cplusplus
extern "C" {
#endif

int algo_init();
void algo_close(int err);
char *check_algo_version();
void _algo_process1(const short *mic_data, const short *ref_data, short *out_data, const int nums);
void _algo_process2(const short *mic_data, const short *ref_data, const int nums);
void _algo_process3(const short *mic_data, const short *ref_data, const int nums);

#ifdef __cplusplus
}
#endif

#endif // ALGO_H