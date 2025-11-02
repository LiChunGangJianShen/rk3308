#ifndef AES_REPORT_H
#define AES_REPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define RECV_PORT   59128
#define SEND_PORT   57168

#define OUT_REPORT_ID	0x60	//host-2-dev
#define IN_REPORT_ID  0x61		//dev-2-host
#define HID_REPORT_LENGTH   1024
#define HID_REPORT_DATA_LENGTH   1024-6
#define HID_DEV "/dev/hidg0"

typedef enum
{
    cmmd_request_auth = 0,
    cmmd_do_auth,
    cmmd_request_devinfo,
    cmmd_udp_request,
    cmmd_udp_respone,
    cmmd_success_auth,
    cmmd_fail_auth,
    cmmd_already_auth,
    cmmd_max,
} e_auth_tool_cmmd_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t id;
    uint8_t cmmd;
    uint16_t data_size;
    uint8_t data[HID_REPORT_DATA_LENGTH];
    uint16_t crc;
}st_auth_tool_report_t;
#pragma pack(pop)

bool check_encrypt();
int aes_udp_task_init(int cpu, int priority);
void aes_udp_task_destroy();
int hid_task_init(int cpu, int priority);
void hid_task_destroy();

#ifdef __cplusplus  
}
#endif

#endif