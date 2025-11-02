#ifndef GEN_OPT_H
#define GEN_OPT_H

#ifdef  __cplusplus
extern "C" {
#endif

int load_all_user_key(unsigned char *, unsigned char *, unsigned char *, int);
int verify_func(unsigned char *, unsigned char *, unsigned char *, int);
int eeprom_test(unsigned char *, unsigned char *, unsigned char *, int, int, int);

#ifdef  __cplusplus
}
#endif

#endif
