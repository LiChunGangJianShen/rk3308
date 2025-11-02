#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define I2C_DEV "/dev/i2c"
extern int g_i2c_bus;

int eep_i2c_read(unsigned char device_addr, unsigned char *sub_addr, unsigned char *buff, unsigned int ByteNo)
{
	int fd, ret;
	// int i;
	char devname[64] = {0};
	struct i2c_rdwr_ioctl_data read_data;

	sprintf(devname, "/dev/i2c-%d", g_i2c_bus);
	fd = open(devname, O_RDWR);
	if(fd < 0)
	{
		//perror("open error");
		return 0;
	}
	device_addr >>= 1;
	
	read_data.nmsgs = 2;
	read_data.msgs = (struct i2c_msg *)malloc(read_data.nmsgs * sizeof(struct i2c_msg));
	if(NULL == read_data.msgs)
	{
		//perror("malloc error");
		close(fd);
		return -1;
	}
	read_data.msgs[0].len = 2;
	read_data.msgs[0].addr = device_addr;
	read_data.msgs[0].flags = 0;
	read_data.msgs[0].buf = (unsigned char *)malloc(read_data.msgs[0].len);
	read_data.msgs[0].buf[0] = sub_addr[0];
	read_data.msgs[0].buf[1] = sub_addr[1];

	read_data.msgs[1].len = ByteNo;
	read_data.msgs[1].addr = device_addr;
	read_data.msgs[1].flags = 1;
	read_data.msgs[1].buf = buff;

	ret = ioctl(fd, I2C_RDWR, (unsigned long)&read_data);
	if(ret < 0)
	{
		perror("read error\n");
		if(read_data.msgs[0].buf)
			free(read_data.msgs[0].buf);
		if(read_data.msgs)
			free(read_data.msgs);
		close(fd);
		return -1;
	}
	// printf("read from device: 0x%2x  reg: 0x%x\n", device_addr, (sub_addr[0]<<8|sub_addr[1]));
	// printf("read data:\n");
	// for(i = 0; i < ByteNo; i++)
	// printf("0x%2x\t", buff[i]);
	// printf("\n");
	
	close(fd);

	if(read_data.msgs[0].buf)
		free(read_data.msgs[0].buf);
	if(read_data.msgs)
		free(read_data.msgs);

	return 0;
}

int eep_i2c_write(unsigned char device_addr, unsigned char *sub_addr, unsigned char *buff, unsigned int ByteNo)
{
	int fd, ret, i;
	char devname[64] = {0};
	struct i2c_rdwr_ioctl_data write_data;

	sprintf(devname, "/dev/i2c-%d", g_i2c_bus);
	fd = open(devname, O_RDWR);
	if(fd < 0)
	{
		//perror("open error");
		return 0;
	}
	device_addr >>= 1;

	write_data.nmsgs = 1;
	write_data.msgs = (struct i2c_msg *)malloc(write_data.nmsgs * sizeof(struct i2c_msg));
	if(NULL == write_data.msgs)
	{
		//perror("malloc error");
		close(fd);
		return -1;
	}
	write_data.msgs[0].len = ByteNo + 2;
	write_data.msgs[0].addr = device_addr;
	write_data.msgs[0].flags = 0;
	write_data.msgs[0].buf = (unsigned char *)malloc(write_data.msgs[0].len);
	write_data.msgs[0].buf[0] = sub_addr[0];
	write_data.msgs[0].buf[1] = sub_addr[1];
	for(i = 0; i < ByteNo; i++)
	{
		write_data.msgs[0].buf[i + 2] = buff[i];
	}
	ret = ioctl(fd, I2C_RDWR, (unsigned long)&write_data);
	if(ret < 0)
	{
		//perror("write error");
		if(write_data.msgs[0].buf)
			free(write_data.msgs[0].buf);
		if(write_data.msgs)
			free(write_data.msgs);
		close(fd);
		return -1;
	}
	// printf("write to device: 0x%2x reg: 0x%x\n", device_addr, (sub_addr[0]<<8|sub_addr[1]));
	// printf("write data:\n");
	// for(i = 0; i < ByteNo; i++)
	// printf("0x%2x\t", buff[i]);
	// printf("\n");

	close(fd);

	if(write_data.msgs[0].buf)
		free(write_data.msgs[0].buf);
	if(write_data.msgs)
		free(write_data.msgs);

	return 0;
}

#ifdef __cplusplus
}
#endif