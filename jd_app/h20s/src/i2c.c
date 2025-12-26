#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "log.h"

int i2c_init(const char *i2c_dev)
{
    if(NULL == i2c_dev){
        loge("invalid i2c device\n");
        return -1;
    }

    int fd = open(i2c_dev, O_RDWR);
    if(fd < 0){
        loge("open i2c device %s failed\n", i2c_dev);
        return -1;
    }

    return fd;
}

void i2c_uninit(int i2c_fd)
{
    close(i2c_fd);
}

int i2cRead(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char* buff, int size)
{
	int rc = 0;
	struct i2c_rdwr_ioctl_data ioctl_data;
	struct i2c_msg msgs[2];

	msgs[0].addr = dev_addr;
	msgs[0].flags = 0;//write
	msgs[0].len = sizeof(reg_addr);
	msgs[0].buf = &reg_addr;

	msgs[1].addr = dev_addr;
	msgs[1].flags = 1;//read
	msgs[1].len = size;
	msgs[1].buf = buff;

	ioctl_data.msgs = msgs;
	ioctl_data.nmsgs = 2;

	rc = ioctl(fd, I2C_RDWR|I2C_SLAVE_FORCE, (unsigned long)&ioctl_data);
	if(rc < 0){
		loge("rc=%d, read 0x%x failed\n", rc, dev_addr);
	}

	return rc;
}

int i2cWrite(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char* buff, int size)
{
	int rc = 0;
	struct i2c_rdwr_ioctl_data ioctl_data;
	struct i2c_msg msg;

	unsigned char *buf = malloc(size + 1);
	if (!buf) {
        perror("malloc failed");
        return -1;
    }

	buf[0] = reg_addr;
    memcpy(buf + 1, buff, size);

	msg.addr = dev_addr;
	msg.flags = 0;//write
	msg.len = size+1;
	msg.buf = buf;

	ioctl_data.msgs = &msg;
	ioctl_data.nmsgs = 1;

	rc = ioctl(fd, I2C_RDWR|I2C_SLAVE_FORCE, (unsigned long)&ioctl_data);
	if(rc < 0){
		loge("rc=%d, write 0x%x failed\n", rc, dev_addr);
	}
	free(buf);

	return rc;
}