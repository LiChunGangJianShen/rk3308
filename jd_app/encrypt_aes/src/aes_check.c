#include "rk_vendor_storage.h"
#include "rk_serial_num.h"
#include "aes.h"
#include "debug.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool check_encrypt()
{
    /********************************************************/
    //for test
    char buf[1024];
    AES256_ctx ctx;
    memset(buf, 0, sizeof(buf));
    vendor_storage_read(buf, sizeof(buf));
    DEBUG_PRINT("read vendor storage: %s\n", buf);
    aes256_init_ctx(&ctx);
    aes256_ecb_decrypt(&ctx, (unsigned char *)buf);
    DEBUG_PRINT("decrypt: %s\n", buf);
    /********************************************************/

    char sn[1024];
    memset(sn, 0, sizeof(sn));
    check_cpu_serial_num(sn, sizeof(sn));
    DEBUG_PRINT("SN:\n");
    DEBUG_PRINT("%s\n", sn);

    if(strcmp(sn, buf) == 0){
        DEBUG_PRINT("is valid encrypt\n");
        return true;
    }
    return false;
}

#ifdef __cplusplus
}
#endif