#ifdef  __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "gen_fa.h"
#include "gen_opt.h"
#include "i2c.h"
#include "h20_encrypt.h"

#define I2C_DEV_ADDR    0x82
int g_i2c_bus = 3;
		
extern unsigned char USER_KEY[USER_KEY_LENGTH];
unsigned char UKEY1[USER_KEY_LENGTH] = 
{ 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};
unsigned char UKEY2[USER_KEY_LENGTH] = 
{ 
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
};
unsigned char UKEY3[USER_KEY_LENGTH] = 
{ 
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

//user key verify
bool h20_gen_fa_verify(void)
{
    ucDevAddr = I2C_DEV_ADDR;
    if(verify_func(UKEY1, UKEY2, UKEY3, USER_KEY_LENGTH)){
        printf("verify fail\n");
        return false;
    }
    
    printf("verify success\n");
    return true;
}

//user key set
bool h20_gen_fa_program(void)
{
    ucDevAddr = I2C_DEV_ADDR;
    if(load_all_user_key(UKEY1, UKEY2, UKEY3, USER_KEY_LENGTH)){
        printf("load user key fail\n");
        return false;
    }
    
    printf("load user key success\n");
    return true;
}

//user key set & verify 
bool h20_gen_fa_test(void)
{
    ucDevAddr = I2C_DEV_ADDR;
    if(load_all_user_key(UKEY1, UKEY2, UKEY3, USER_KEY_LENGTH)){
        printf("load user key fail\n");
        return false;
    }
    
    if(verify_func(UKEY1, UKEY2, UKEY3, USER_KEY_LENGTH)){
        printf("verify fail\n");
        return false;
    }
    
    printf("load user key & verify success\n");
    return true;
}

//eeprom write & read
bool h20_gen_fa_eeprom_test(void)
{
    unsigned char wdata[USER_KEY_LENGTH];
    unsigned char rdata[USER_KEY_LENGTH];
    int i, fail_flag=0;

    ucDevAddr = I2C_DEV_ADDR;
    
    for(i = 0; i < USER_KEY_LENGTH; i++){
        wdata[i] = i;
    }
    eeprom_test(UKEY1, wdata, rdata, USER_KEY_LENGTH, UKEY1_SEL, USER_KEY_LENGTH);
    for(i = 0; i < USER_KEY_LENGTH; i++){
        printf("i:%d, wdata:0x%x, rdata:0x%x\n", i, wdata[i], rdata[i]);
        if(wdata[i] != rdata[i]){
            fail_flag++;
        }
    }
    if(fail_flag){
        printf("eeprom test fail(%d)\n", fail_flag);
        return false;
    }
    
    printf("eeprom test success\n");
    return true;
}

#ifdef  __cplusplus
}
#endif