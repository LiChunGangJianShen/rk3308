#ifndef GEN_FA_H
#define GEN_FA_H

#ifdef  __cplusplus
extern "C" {
#endif

int eep_i2c_read(unsigned char, unsigned char *, unsigned char *, unsigned int);
int eep_i2c_write(unsigned char, unsigned char *, unsigned char *, unsigned int);

#ifdef  __cplusplus
}
#endif

#endif
