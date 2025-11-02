#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VENDOR_REQ_TAG      0x56524551
#define VENDOR_READ_IO      _IOW('v', 0x01, unsigned int)//_IOWR('v', 0x01, unsigned int)
#define VENDOR_WRITE_IO     _IOW('v', 0x02, unsigned int)
#define VENDOR_ID   12

#define VENDOR_DEV  "/dev/vendor_storage"

struct rk_vendor_req {
    uint32_t tag;
    uint16_t id;
    uint16_t len;
    uint8_t data[1024];//Equipment model + CPU-SN, example jd800-4ed1503b0789b19b
};

int vendor_storage_erase()
{
    int err = 0;
    struct rk_vendor_req req;

    int fd = open(VENDOR_DEV, O_RDWR, 0);
    if(fd < 0){
        DEBUG_PRINT("open %s fail\n", VENDOR_DEV);
        return fd;
    }

    memset(&req, 0, sizeof(struct rk_vendor_req));
    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_ID;
    req.len = sizeof(req.data);
    memset(req.data, 0, req.len);
    err = ioctl(fd, VENDOR_WRITE_IO, &req);
    if(err) {
        DEBUG_PRINT("earse fail, err=%d, none\n", err);
        close(err);
        return -2;
    }

    close(fd);
    return err;
}

int vendor_storage_write(const char *data)
{
    int err = 0;
    struct rk_vendor_req req;

    int fd = open(VENDOR_DEV, O_RDWR, 0);
    if(fd < 0){
        DEBUG_PRINT("open %s fail\n", VENDOR_DEV);
        return fd;
    }

    memset(&req, 0, sizeof(struct rk_vendor_req));
    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_ID;
    req.len = strlen(data);
    sprintf((char *)req.data, "%s", data);
    DEBUG_PRINT("(%s) write data: %s\n", data, req.data);
    err = ioctl(fd, VENDOR_WRITE_IO, &req);
    if(err) {
        DEBUG_PRINT("write fail, err=%d, none\n", err);
        close(err);
        return -2;
    }

    close(fd);
    return err;
}

int vendor_storage_read(char data[], int len)
{
    int err = 0;
    struct rk_vendor_req req;

    int fd = open(VENDOR_DEV, O_RDWR, 0);
    if(fd < 0){
        DEBUG_PRINT("open %s fail\n", VENDOR_DEV);
        return fd;
    }

    memset(&req, 0, sizeof(struct rk_vendor_req));
    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_ID;
    req.len = sizeof(req.data);
    err = ioctl(fd, VENDOR_READ_IO, &req);
    if(err) {
        DEBUG_PRINT("read fail, err=%d, none\n", err);
        close(err);
        return -2;
    }

    memcpy(data, req.data, len);

    close(fd);
    return err;
}

#ifdef __cplusplus
}
#endif