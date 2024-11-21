#ifndef __I2C_H
#define __I2C_H

#ifdef __cplusplus
extern "C" {
#endif

int i2c_init(const char *i2c_dev);
void i2c_uninit(int i2c_fd);
int i2cRead(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char* buff, int size);
int i2cWrite(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char* buff, int size);

#ifdef __cplusplus
}
#endif

#endif