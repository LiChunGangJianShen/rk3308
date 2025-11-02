#ifndef HID_H
#define HID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OUT_REPORT_ID	0x60	//host-2-dev
#define IN_REPORT_ID  0x61		//dev-2-host
#define HID_REPORT_LENGTH   1024
#define HID_REPORT_DATA_LENGTH   1024-6
#define HID_DEV "/dev/hidg0"

typedef enum
{
    cmmd_test = 0,
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

int hid_task_init(int cpu, int priority);
void hid_task_destroy();

#ifdef __cplusplus  
}
#endif

#endif