#ifndef _CRC_H
#define _CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t cyg_crc16(unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif
#endif /* _UBOOT_CRC_H */
