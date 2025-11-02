#ifndef GEN_FA_H
#define GEN_FA_H

#ifdef  __cplusplus
extern "C" {
#endif

#define UKEY1_SEL       1       //--- addr range : 0x0000 ~ 0x03FF
#define UKEY2_SEL       2       //--- addr range : 0x0400 ~ 0x07FF
#define UKEY3_SEL       3       //--- addr range : 0x0800 ~ 0x0CFF

#define USER_KEY_LENGTH     16  //--- Length : Kind of Key Length (16, 32, 48, 64)

extern unsigned char ucDevAddr;

extern void delay_ms(unsigned int dly);
extern unsigned char _alpu_rand(void);
extern int eep_i2c_write(unsigned char dev_addr, unsigned char *addr, unsigned char *tx_data, unsigned int ByteLen);
extern int eep_i2c_read (unsigned char dev_addr, unsigned char *addr, unsigned char *tx_data, unsigned int ByteLen);

extern unsigned char SET_USER_KEY(unsigned char *ukey, int UserKey_Sel );
extern unsigned char AUTHENTICATION(unsigned char *tx_data, int UserKey_Sel);
extern unsigned char SHA_AUTH_MODE(unsigned char * sht_pt,unsigned char * ucShaS3Out,unsigned char * uclShaS3OutMcu );
extern unsigned char eep_write(unsigned char *addr, unsigned char *w_data, int data_len, int UserKey_Sel);
extern unsigned char eep_read(unsigned char *addr, unsigned char *w_data, int data_len, int UserKey_Sel);

extern unsigned char USER_KEY[USER_KEY_LENGTH];

#ifdef  __cplusplus
}
#endif

#endif
