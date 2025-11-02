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

static pthread_state_t state_aes_udp_task;

static void report_handle(st_auth_tool_report_t *report, int sock, struct sockaddr_in *addr)
{
    st_auth_tool_report_t send_report;
    memset(&send_report, 0, sizeof(st_auth_tool_report_t));
    memcpy(&send_report, report, sizeof(st_auth_tool_report_t));
    
    report->crc = ntohs(report->crc);
    uint16_t calculate_crc = cyg_crc16((uint8_t *)report, sizeof(st_auth_tool_report_t)-2);
    report->data_size = ntohs(report->data_size);
    if(report->crc != calculate_crc){
        DEBUG_PRINT("crc error[recv: 0x%x, calculate: 0x%x]\n", report->crc, calculate_crc);
    }

    if(report->cmmd == cmmd_udp_request){
        DEBUG_PRINT("udp request\n");
        memset(&send_report, 0, sizeof(st_auth_tool_report_t));
        send_report.cmmd = cmmd_udp_respone;
    }
    else if(report->cmmd == cmmd_request_devinfo){
        DEBUG_PRINT("device info request\n");
        memset(send_report.data, 0, sizeof(send_report.data));
        check_cpu_serial_num((char *)send_report.data, sizeof(send_report.data));
        send_report.data_size = strlen((char *)send_report.data)+1;
        send_report.data_size = htons(send_report.data_size);
    }
    else if(report->cmmd == cmmd_request_auth){
        DEBUG_PRINT("auth request\n");
        memset(&send_report, 0, sizeof(st_auth_tool_report_t));
        if(check_encrypt()){
            DEBUG_PRINT("already auth\n");
            send_report.cmmd = cmmd_already_auth;
        }
        else{
            DEBUG_PRINT("to do auth\n");
            send_report.cmmd = cmmd_do_auth;
            memset(report->data, 0, sizeof(report->data));
            strcpy((char *)report->data, AES_FEATURE_ID);
            report->data_size = strlen(AES_FEATURE_ID);
        }
        check_cpu_serial_num((char *)send_report.data, sizeof(send_report.data));
        send_report.data_size = strlen((char *)send_report.data)+1;
        send_report.data_size = htons(send_report.data_size);
    }
    else if(report->cmmd == cmmd_do_auth){
        DEBUG_PRINT("to auth\n");
        AES256_ctx ctx;
        char buf[1018] = {0};
        aes256_init_ctx(&ctx);
        memcpy(buf, report->data, sizeof(buf));
        DEBUG_PRINT("before encrypt: %s\n", buf);
        aes256_ecb_encrypt(&ctx, (unsigned char *)buf);
        DEBUG_PRINT("after encrypt: %s\n", buf);
        int err = vendor_storage_write((char *)buf);
        
        memset(&send_report, 0, sizeof(st_auth_tool_report_t));
        if(err == 0 && check_encrypt())
            send_report.cmmd = cmmd_success_auth;
        else
            send_report.cmmd = cmmd_fail_auth;
        
    }

    send_report.crc = cyg_crc16((uint8_t *)&send_report, sizeof(st_auth_tool_report_t)-2);
    send_report.crc = htons(send_report.crc);
    int ret = udp_socket_send(sock, &send_report, sizeof(send_report), SEND_PORT, addr);
    if(ret > 0){
        DEBUG_PRINT("send success\n");
    }
}

static int aes_udp_task(void *arg)
{
    prctl(PR_SET_NAME, "aes_udp_task");
    DEBUG_PRINT("aes udp task start\n");

    int recv_sock = udp_socket_init(RECV_PORT, 1);
    if(recv_sock < 0){
        DEBUG_PRINT("udp recv sock init fail\n");
        goto _err_recv_init;
    }

    int send_sock = udp_socket_init(SEND_PORT, 0);
    if(send_sock < 0){
        DEBUG_PRINT("udp send sock init fail\n");
        goto _err_send_init;
    }

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    addr.sin_addr.s_addr = INADDR_ANY;
    int ret = 0;    
    st_auth_tool_report_t report;

    while(state_aes_udp_task.running){
        memset(&report, 0, sizeof(report));
        ret = udp_socket_recv(recv_sock, &report, sizeof(report), &addr, &addr_len);
        if(ret > 0){
            DEBUG_PRINT("recv from: %s\n", inet_ntoa(addr.sin_addr));
            DEBUG_PRINT("recv data: %s\n", report.data);
            
            report_handle(&report, send_sock, &addr);
        }
        usleep(100);
    }

    udp_socket_delete(send_sock);
_err_send_init:
    udp_socket_delete(recv_sock);
_err_recv_init:
    DEBUG_PRINT("aes udp task stop\n");
    return 0;
}

int aes_udp_task_init(int cpu, int priority)
{
#if defined(RK3562) || defined(RK3308)
    if(cpu < 0 || cpu > 3){
        DEBUG_PRINT("invalid cpu, shuild be in[0, 3]\n");
        return -1;
    }
#endif
    int err = create_thread("aes_udp_task", cpu, priority, aes_udp_task, &state_aes_udp_task);
    if(err < 0){
        DEBUG_PRINT("create aes_udp_task fail\n");
    }
    return err; 
}

void aes_udp_task_destroy()
{
    destroy_thread(&state_aes_udp_task);
}

#ifdef __cplusplus  
}
#endif