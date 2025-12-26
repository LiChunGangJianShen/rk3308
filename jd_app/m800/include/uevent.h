#ifndef UEVENT_H
#define UEVENT_H

#ifdef __cplusplus
extern "C" {
#endif

int init_uevent_listen();
void destroy_event_listen();
int check_usb_connect();
int check_usb_disconnect();
int check_usb_configured();
int check_stream_out_state();
int check_stream_in_state();

#ifdef __cplusplus
}
#endif

#endif // UEVENT_H