#ifndef AES_H
#define AES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AES_BLOCKLEN 16  // 16字节块大小
#define AES_KEYLEN 32    // 32字节密钥长度
#define AES_keyExpSize 240

#define AES_FEATURE_ID  "8bac97e2"

typedef struct {
    uint8_t round_key[AES_keyExpSize];
} AES256_ctx;

void aes256_init_ctx(AES256_ctx *ctx);
void aes256_ecb_encrypt(const AES256_ctx *ctx, uint8_t *buf);
void aes256_ecb_decrypt(const AES256_ctx *ctx, uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif