#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "gen_fa.h"

void delay_ms(unsigned int dly)
{
    usleep(1000 * dly);
}

unsigned char _alpu_rand(void)
{

  static unsigned long seed; // 2byte, must be a static variable

  seed = seed + rand(); // rand(); <------------------ add time value
  seed =  seed * 1103515245 + 12345;

  return (seed/65536) % 32768;

}

static unsigned char sha_auth_example(void)
{
	int i;

	unsigned char error_code;
    unsigned char pt_data[16];
    unsigned char ShaS3Out[32];
    unsigned char ShaS3OutMcu[32];

    for ( i=0; i<16; i++ ) pt_data[i] = _alpu_rand();

    error_code = SHA_AUTH_MODE(pt_data, ShaS3OutMcu, ShaS3Out);
  	// if ( error_code ) printf("\r\n SHA256 AUTH Test Fail!!");
	// else printf("\r\n SHA256 AUTH Test Success!!!");
    // printf("\r\n Result     : %d",error_code);
    // printf("\r\n Answer     : ");for (i=0; i<32; i++) printf("0x%2x ", ShaS3Out[i]);
    // printf("\r\n Dx_Data    : ");for (i=0; i<32; i++) printf("0x%2x ", ShaS3OutMcu[i]);
  
	return error_code;
}

static int load_user_key(unsigned char *user_key, int keysel, int userkey_length)
{
	int i, error_code;
	
	if(keysel < UKEY1_SEL || keysel > UKEY3_SEL){
		printf("[%s-%d]invalid keysel(%d)\n", __func__, __LINE__, keysel);
		return -1;
	}
	
	for(i = 0; i < userkey_length; i++){
		USER_KEY[i] = user_key[i];
	}
	
	error_code = SET_USER_KEY(USER_KEY, keysel);
	if(error_code)
		printf("load user_key-%d fail\n", keysel);
	else
		printf("load user_key-%d success\n", keysel);
	
	return error_code;
}

int load_all_user_key(unsigned char *user_key1, unsigned char *user_key2, unsigned char *user_key3, int userkey_length)
{
	int ret = 0;
	
	ret = load_user_key(user_key1, UKEY1_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]load user_key-%d fail\n", __func__, __LINE__, UKEY1_SEL);
		return ret;
	}
	
	ret = load_user_key(user_key2, UKEY2_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]load user_key-%d fail\n", __func__, __LINE__, UKEY2_SEL);
		return ret;
	}
	
	ret = load_user_key(user_key3, UKEY3_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]load user_key-%d fail\n", __func__, __LINE__, UKEY3_SEL);
	}
	
	return ret;
}

static int user_key_authentication(unsigned char *user_key, int keysel, int userkey_length)
{
	int i, error_code;
	unsigned char *tx_data = (unsigned char *)malloc(userkey_length);
	
	if(keysel < UKEY1_SEL || keysel > UKEY3_SEL){
		printf("[%s-%d]invalid keysel(%d)\n", __func__, __LINE__, keysel);
		return -1;
	}
	
	for(i = 0; i < userkey_length; i++){
		tx_data[i] = _alpu_rand();
		USER_KEY[i] = user_key[i];
	}
	
	error_code = AUTHENTICATION(tx_data, keysel);
	// if(error_code)
		// printf("user_key-%d authentication fail\n", keysel);
	// else
		// printf("user_key-%d authentication success\n", keysel);
	
	if(tx_data){
		free(tx_data);
		tx_data = NULL;
	}
	
	return error_code;
}

static int all_user_key_authentication(unsigned char *user_key1, unsigned char *user_key2, unsigned char *user_key3, int userkey_length)
{
	int ret = 0;
	
	ret = user_key_authentication(user_key1, UKEY1_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]authentication user_key-%d fail\n", __func__, __LINE__, UKEY1_SEL);
		return ret;
	}
	
	ret = user_key_authentication(user_key2, UKEY2_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]authentication user_key-%d fail\n", __func__, __LINE__, UKEY2_SEL);
		return ret;
	}
	
	ret = user_key_authentication(user_key3, UKEY3_SEL, userkey_length);
	if(ret){
		printf("[%s-%d]authentication user_key-%d fail\n", __func__, __LINE__, UKEY3_SEL);
	}
	
	return ret;
}

int verify_func(unsigned char *user_key1, unsigned char *user_key2, unsigned char *user_key3, int userkey_length)
{
	int ret = 0;
	
	ret = all_user_key_authentication(user_key1, user_key2, user_key3, userkey_length);
	if(ret){
		printf("user-key verify fail\n");
		return ret;
	}
	
	ret = sha_auth_example();
	if(ret){
		printf("Original key verify fail\n");
	}
	
	return ret;
}

int eeprom_test(unsigned char *user_key, unsigned char *wdata, unsigned char *rdata, int data_len, int keysel, int userkey_length)
{
	unsigned char addr[2] = {0};
	int i, ret;
	
	if(keysel < UKEY1_SEL || keysel > UKEY3_SEL){
		printf("[%s-%d]invalid keysel(%d)\n", __func__, __LINE__, keysel);
		return -1;
	}
	
	if(keysel == UKEY1_SEL){
		addr[0] = 0x00;
		addr[1] = 0x00;
	}
	else if(keysel == UKEY2_SEL){
		addr[0] = 0x04;
		addr[1] = 0x00;
	}
	else if(keysel == UKEY3_SEL){
		addr[0] = 0x08;
		addr[1] = 0x00;
	}
	
	printf("keysel: %d, addr0:0x%x, addr1:0x%x\n", keysel, addr[0], addr[1]);
	printf("eeprom write user-key:\n");
	for(i = 0; i < userkey_length; i++){
		USER_KEY[i] = user_key[i];
		printf("0x%x ", USER_KEY[i]);
	}
	printf("\n\n");
	ret = eep_write(addr, wdata, data_len, keysel);
	// if(ret)
	// 	printf("eeprom write fail\n");
	// else
	// 	printf("eeprom write success\n");
	
	printf("eeprom read user-key:\n");
	for(i = 0; i < userkey_length; i++){
		USER_KEY[i] = user_key[i];
		printf("0x%x ", USER_KEY[i]);
	}
	printf("\n\n");
	ret = eep_read(addr, rdata, data_len, keysel);
	// if(ret)
	// 	printf("eeprom read fail\n");
	// else
	// 	printf("eeprom read success\n");
	
	return ret;
}

#ifdef  __cplusplus
}
#endif