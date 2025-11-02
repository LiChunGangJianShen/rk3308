#ifndef SERIAL_H
#define SERIAL_H

#ifdef  __cplusplus
extern "C" {
#endif

int serial_init(const char *dev, int baud_rate);
int serial_exit(int fd);

#ifdef  __cplusplus
}
#endif

#endif