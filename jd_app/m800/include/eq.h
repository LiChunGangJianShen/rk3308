#ifndef EQ_H
#define EQ_H

#ifdef __cplusplus
extern "C" {
#endif

#define EQ_BAND 16

int save_eq(float *eq, int len);
void algo_eq_init(void);
void check_eq(float eq_buf[], int len);

#ifdef __cplusplus
}
#endif

#endif // EQ_H