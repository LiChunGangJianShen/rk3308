#ifndef RK_VENDOR_STORAGE_H
#define RK_VENDOR_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

int vendor_storage_write(const char *data);
int vendor_storage_read(char data[], int len);
int vendor_storage_erase();

#ifdef __cplusplus
}
#endif
#endif