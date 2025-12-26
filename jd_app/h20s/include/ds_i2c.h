#ifndef DS_I2C_H
#define DS_I2C_H

#ifdef  __cplusplus
extern "C" {
#endif

int i2c_open(const char *device);
void i2c_close(int fd);
int i2c_read(int fd, int dev_addr, uint8_t reg, void *buf, size_t len);
int i2c_write(int fd, int dev_addr, uint8_t reg, const void *data, size_t len);

#ifdef  __cplusplus
}
#endif

#endif