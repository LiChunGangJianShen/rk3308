#include "thread.h"
#include "crc.h"
#include "log.h"
#include "hid.h"
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static pthread_state_t state_hid_task;

static int hid_open(const char *dev)
{
    if(!dev)
        return -1;
    
    int fd = open(dev, O_RDWR, 0666);
    if(fd < 0){
        loge("hid-dev(%s) open fail\n", dev);
        return -1;
    }
    return fd;
}

static bool is_fd_valid(int fd)
{
    int err = fcntl(fd, F_GETFL);
    if(err == -1 || errno == EBADF)
        return false;

    return true;
}

static void hid_close(int fd)
{
    if(is_fd_valid(fd))
        close(fd);
}

static int hid_init(const char *dev)
{
    return hid_open(dev);
}

static void hid_destroy(int fd)
{
    hid_close(fd);
}

static int hid_report_send(int fd, void *data, int data_len)
{
    if(!is_fd_valid(fd))
        return -1;
    int err = 0;
    err = write(fd, data, data_len);
    if(err != data_len){
        logw("hid report send fail\n");
        if(err > 0){
            logw("send(%d)\n", data_len);
        }
        else{
            loge("hid write err\n");
        }
    }

    return err;
}

static int hid_report_recv(int fd, void *data, int data_len)
{
    if(!is_fd_valid(fd))
        return -1;
    int err = read(fd, data, data_len);
    if(err < 0){
        loge("hid report recv fail\n");
    }

    return err;
}

static void hid_report_handle(int fd, st_auth_tool_report_t *report)
{
    uint16_t checksum = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
    logi("checksum=0x%x, report->crc=0x%x\n", checksum, report->crc);
	if(report->crc != checksum){	
		logw("hid tool CRC checksum error\n");	
		return;
	}
    if(report->id != OUT_REPORT_ID){
		logw("invalid report id(0x%x)\n", report->id);
		return;
	}
    switch (report->cmmd)
    {
    case cmmd_test:
        {
            logi("cmmd_test\n");
            memset(report, 0, sizeof(st_auth_tool_report_t));
            report->cmmd = cmmd_test;
            strcpy((char *)report->data, "cmmd_test_ack");
            report->data_size = strlen("cmmd_test_ack");
            report->id = IN_REPORT_ID;
            report->crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
            hid_report_send(fd, report, sizeof(st_auth_tool_report_t));
        }
        break;
    default:
        logi("invalid cmmd\n");
        break;
    }
}

static int hid_task(void *arg)
{
    prctl(PR_SET_NAME, "hid_task");
    logi("hid task start\n");

    int ret = 0, recv_len = 0;
    fd_set rfds;
    struct timeval tv;
    st_auth_tool_report_t report_buf;

    int fd = hid_init(HID_DEV);
    if(fd < 0){
        loge("hid init fail\n");
        return -1;
    }
    
    while(state_hid_task.running){
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if(ret > 0){
            if(FD_ISSET(fd, &rfds)){
                recv_len = hid_report_recv(fd, &report_buf, sizeof(st_auth_tool_report_t));
                if(recv_len > 0){
                    hid_report_handle(fd, &report_buf);
                }
            }
        }

        usleep(100);
    }

    hid_destroy(fd);

    return 0;
}

int hid_task_init(int cpu, int priority)
{
    int err = create_thread("hid_task", cpu, priority, hid_task, &state_hid_task);
    if(err < 0){
        loge("create hid_task fail\n");
    }
    return err; 
}

void hid_task_destroy()
{
    destroy_thread(&state_hid_task);
}

#ifdef __cplusplus  
}
#endif