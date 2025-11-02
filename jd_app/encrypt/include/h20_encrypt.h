#ifndef H20_ENCRYPT_H
#define H20_ENCRYPT_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool h20_gen_fa_verify(void);
bool h20_gen_fa_program(void);
bool h20_gen_fa_test(void);
bool h20_gen_fa_eeprom_test(void);

#ifdef  __cplusplus
}
#endif

#endif
