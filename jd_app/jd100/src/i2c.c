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

#define WR_OPT	0
#define RD_OPT	1

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
	int rc = -1;
	struct i2c_rdwr_ioctl_data ioctl_data;
	
	ioctl_data.nmsgs = 2;
	ioctl_data.msgs = (struct i2c_msg *)malloc(ioctl_data.nmsgs * sizeof(struct i2c_msg));
    if(NULL == ioctl_data.msgs){
        loge("malloc failed for read msgs\n");
		return -1;
    }
	
	ioctl_data.msgs[0].len = 1;
	ioctl_data.msgs[0].addr = dev_addr;
	ioctl_data.msgs[0].flags = WR_OPT;
	ioctl_data.msgs[0].buf = (unsigned char *)malloc(1);
	ioctl_data.msgs[0].buf[0] = reg_addr;
	
    ioctl_data.msgs[1].len = size;
	ioctl_data.msgs[1].addr = dev_addr;
	ioctl_data.msgs[1].flags = RD_OPT;
	ioctl_data.msgs[1].buf = buff;
	
	rc = ioctl(fd, I2C_RDWR|I2C_SLAVE_FORCE, (unsigned long)&ioctl_data);
	if(rc < 0){
		loge("rc=%d, read 0x%x failed\n", rc, dev_addr);
        goto __free;
	}
	
	// for(int i = 0; i < size; i++){
		// logd("read dev:0x%x, reg:0x%x, value:0x%x  success\n", dev_addr, reg_addr, buff[i]);
	// }

__free:
	if(ioctl_data.msgs[0].buf)
		free(ioctl_data.msgs[0].buf);
	if(ioctl_data.msgs)	
		free(ioctl_data.msgs);
	
	return rc;
}

int i2cWrite(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char* buff, int size)
{
	int rc = -1;
	struct i2c_rdwr_ioctl_data ioctl_data;
	
	ioctl_data.nmsgs = 1;
	ioctl_data.msgs = (struct i2c_msg *)malloc(ioctl_data.nmsgs * sizeof(struct i2c_msg));
    if(NULL == ioctl_data.msgs){
        loge("malloc failed for write msgs\n");
		return -1;
    }
	
	ioctl_data.msgs[0].len = size + 1;
	ioctl_data.msgs[0].addr = dev_addr;
	ioctl_data.msgs[0].flags = WR_OPT;
	ioctl_data.msgs[0].buf = (unsigned char *)malloc(size + 1);
	ioctl_data.msgs[0].buf[0] = reg_addr;
	for(int i = 0; i < size; i++)
	{
		ioctl_data.msgs[0].buf[i + 1] = buff[i];
	}
	
	rc = ioctl(fd, I2C_RDWR|I2C_SLAVE_FORCE, (unsigned long)&ioctl_data);
	if(rc < 0){
		loge("rc=%d, write 0x%x failed\n", rc, dev_addr);
        goto __free;
	}
	
	// for(int i = 0; i < size; i++){
		// logd("write dev:0x%x, reg:0x%x, value:0x%x, success\n", dev_addr, reg_addr, buff[i]);
	// }

__free:
	if(ioctl_data.msgs)	
		free(ioctl_data.msgs);
	
	return rc;
}