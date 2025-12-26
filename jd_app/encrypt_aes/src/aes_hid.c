#include "rk_serial_num.h"
#include "rk_vendor_storage.h"
#include "aes.h"
#include "thread.h"
#include "udp.h"
#include "crc.h"
#include "debug.h"
#include "aes_report.h"
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
        DEBUG_PRINT("hid-dev(%s) open fail\n", dev);
        return -1;
    }
    return fd;
}

static bool is_fd_valid(int fd)
{
    int err = fcntl(fd, F_GETFL);
    //if(err == -1 || errno == EBADF)//谨慎使用全局变量errno
    if(err == -1){
        return false;
    }

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
        DEBUG_PRINT("hid report send fail\n");
        if(err > 0){
            DEBUG_PRINT("send(%d)\n", data_len);
        }
        else{
            DEBUG_PRINT("hid write err\n");
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
        DEBUG_PRINT("hid report recv fail\n");
    }

    return err;
}

static void hid_report_handle(int fd, st_auth_tool_report_t *report)
{
    uint16_t checksum = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
    DEBUG_PRINT("checksum=0x%x, report->crc=0x%x\n", checksum, report->crc);
	if(report->crc != checksum){	
		DEBUG_PRINT("hid tool CRC checksum error\n");	
		return;
	}
    if(report->id != OUT_REPORT_ID){
		DEBUG_PRINT("invalid report id(0x%x)\n", report->id);
		return;
	}
    switch (report->cmmd)
    {
    case cmmd_request_auth:
        {
            DEBUG_PRINT("cmmd_request_auth\n");
            // char sn[32] = {0};
            // check_cpu_serial_num(sn, sizeof(sn));
            memset(report, 0, sizeof(st_auth_tool_report_t));
            if(check_encrypt()){
                DEBUG_PRINT("already auth\n");
                report->cmmd = cmmd_already_auth;
            }
            else{
                DEBUG_PRINT("to do auth\n");
                report->cmmd = cmmd_do_auth;
                memset(report->data, 0, sizeof(report->data));
                strcpy((char *)report->data, AES_FEATURE_ID);
                report->data_size = strlen(AES_FEATURE_ID);
            }
            report->id = IN_REPORT_ID;
            report->crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
            int sc = hid_report_send(fd, report, sizeof(st_auth_tool_report_t));
			DEBUG_PRINT("send(%d) cmd: %d, data: %s\n", sc, report->cmmd, report->data);
        }
        break;
    case cmmd_do_auth:
        DEBUG_PRINT("cmmd_do_auth\n");
		DEBUG_PRINT("recv data_size: %d, data: %s\n", report->data_size, report->data);
        if(report->data_size > 0){
            AES256_ctx ctx;
            char buf[1018] = {0};
            aes256_init_ctx(&ctx);
            memcpy(buf, report->data, sizeof(buf));
            DEBUG_PRINT("before encrypt: %s\n", buf);
            aes256_ecb_encrypt(&ctx, (unsigned char *)buf);
            DEBUG_PRINT("after encrypt: %s\n", buf);
            int err = vendor_storage_write((char *)buf);

            if(err == 0 && check_encrypt())
                report->cmmd = cmmd_success_auth;
            else
                report->cmmd = cmmd_fail_auth;
            memset(report->data, 0, sizeof(report->data));
            report->data_size = 0;
            report->id = IN_REPORT_ID;
            report->crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
            int sc = hid_report_send(fd, report, sizeof(st_auth_tool_report_t));
			DEBUG_PRINT("send(%d) cmd: %d, data: %s\n", sc, report->cmmd, report->data);
        }
        break;
    case cmmd_request_devinfo:
        DEBUG_PRINT("cmmd_request_devinfo\n");
        {
            char sn[32] = {0};
            check_cpu_serial_num(sn, sizeof(sn));
            memset(report->data, 0, sizeof(report->data));
            sprintf((char  *)report->data, "%s", sn);
            report->data_size = strlen((char *)report->data) + 1;
            report->id = IN_REPORT_ID;
            report->crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
            hid_report_send(fd, report, sizeof(st_auth_tool_report_t));
        }
        break;
	case cmmd_check_feature_id:
        DEBUG_PRINT("cmmd_check_feature_id\n");
        {
            memset(report->data, 0, sizeof(report->data));
            strcpy((char *)report->data, AES_FEATURE_ID);
            report->data_size = strlen(AES_FEATURE_ID);
            report->id = IN_REPORT_ID;
            report->crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-sizeof(report->crc));
            hid_report_send(fd, report, sizeof(st_auth_tool_report_t));
        }
        break;
    default:
        DEBUG_PRINT("invalid cmmd\n");
        break;
    }
}

static int hid_task(void *arg)
{
    prctl(PR_SET_NAME, "hid_task");
    DEBUG_PRINT("hid task start\n");

    int ret = 0, recv_len = 0;
    fd_set rfds;
    struct timeval tv;
    st_auth_tool_report_t report_buf;

    int fd = hid_init(HID_DEV);
    if(fd < 0){
        DEBUG_PRINT("hid init fail\n");
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
#if defined(RK3562) || defined(RK3308)
    if(cpu < 0 || cpu > 3){
        DEBUG_PRINT("invalid cpu, shuild be in[0, 3]\n");
        return -1;
    }
#endif
    int err = create_thread("hid_task", cpu, priority, hid_task, &state_hid_task);
    if(err < 0){
        DEBUG_PRINT("create hid_task fail\n");
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