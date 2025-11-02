#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include "log.h"

static int serial_open(const char *dev)
{
    int fd = open(dev, O_RDWR|O_NOCTTY|O_NONBLOCK|O_NDELAY);
    fcntl(fd, F_SETFL, 0);
    return fd;
}

static int serial_close(int fd)
{
    // tcflush(fd, TCIOFLUSH);
    return close(fd);
}

static int baud_rate_s(speed_t speed)
{
    switch (speed)
    {
    case B9600:
        return 9600;
    case B115200:
        return 115200;
    default:
        return -1;
    }
}
#if 0
static int serial_setup(int fd, int baud_rate)
{
    struct termios options;
    speed_t ispeed,ospeed;

    memset(&options, 0, sizeof(struct termios));

    //获取当前配置
    if(tcgetattr(fd, &options) != 0){
        loge("serial tcgetattr fail");
        return -1;
    }
    if(cfsetispeed(&options, baud_rate) < 0){
        loge("cfsetispeed fail");
    }
    if(cfsetospeed(&options, baud_rate) < 0){
        loge("cfsetospeed fail");
    }
    ispeed=cfgetispeed(&options);
    ospeed=cfgetospeed(&options);
    logi("uart ispeed=%d, ospeed=%d", baud_rate_s(ispeed), baud_rate_s(ospeed));
    options.c_cflag |= CLOCAL;   //不占用串口
    options.c_cflag |= CREAD;    //使能接收
    options.c_cflag &= ~CSIZE;   //数据位清零
    options.c_cflag |= CS8;      //8位数据
    options.c_cflag &= ~PARENB;  //无校验位
    options.c_cflag &= ~CSTOPB;  //1位停止位
    options.c_cflag &= ~CRTSCTS; //禁止硬件流控
    options.c_cflag &= ~(ICANON|ECHO|ECHOE|ISIG);    //原始输入模式（禁用规范模式处理）
    options.c_cflag &= ~OPOST;   //原始输出模式（禁用输出处理）
    options.c_cc[VMIN] = 1;  //最小读取字符
    options.c_cc[VTIME] = 5;  //等待0.5秒， 单位0.1秒

    //清空输入输出缓冲区
    tcflush(fd, TCIFLUSH);

    //立即应用配置
    if(tcsetattr(fd, TCSANOW, &options) != 0){
        loge("serial tcsetattr fail");
        return -1;
    }

    return 0;
}
#else
static int configure_serial_port(int fd, int baud_rate)
{
    struct termios tty;
    speed_t ispeed,ospeed;
    memset(&tty, 0, sizeof(tty));

    // 获取当前配置（必须成功）
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr failed");
        return -1;
    }

    // 设置波特率（必须使用正确的常量）
    if (cfsetispeed(&tty, baud_rate) != 0 || cfsetospeed(&tty, baud_rate) != 0) {
        perror("cfsetispeed/cfsetospeed failed");
        return -1;
    }
    ispeed=cfgetispeed(&tty);
    ospeed=cfgetospeed(&tty);
    logi("uart ispeed=%d, ospeed=%d\n", baud_rate_s(ispeed), baud_rate_s(ospeed));

    // 数据位、校验位、停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;          // 8 数据位
    tty.c_cflag &= ~PARENB;      // 无校验
    tty.c_cflag &= ~CSTOPB;      // 1 停止位
    tty.c_cflag &= ~CRTSCTS;     // 无硬件流控
    tty.c_cflag &= ~PARODD;     //禁用硬件流控

    // 关键模式设置
    tty.c_cflag |= (CLOCAL | CREAD);  // 必须启用
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // 非规范模式
    tty.c_oflag &= ~OPOST;            // 原始输出

    //关闭输入处理
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);

    // 超时与最小读取字符数（阻塞模式下需设置）
    tty.c_cc[VMIN]  = 1;   // 至少读取 1 字节
    tty.c_cc[VTIME] = 5;   // 超时 0.5 秒

    // 应用配置（必须检查返回值）
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr failed");
        return -1;
    }

    // 清空缓冲区（避免残留数据干扰）
    tcflush(fd, TCIOFLUSH);
    return 0;
}
#endif
int serial_init(const char *dev, int baud_rate)
{
    int fd = serial_open(dev);
    if(fd < 0){
        loge("serial_open fail\n");
        return -1;
    }
#if 0
    if(serial_setup(fd, baud_rate) != 0){
        loge("serial_setup fail\n");
        serial_close(fd);
        return -1;
    }
#else
if(configure_serial_port(fd, baud_rate) != 0){
    loge("configure_serial_port fail\n");
    serial_close(fd);
    return -1;
}
#endif
    return fd;
}

int serial_exit(int fd)
{
    return serial_close(fd);
}