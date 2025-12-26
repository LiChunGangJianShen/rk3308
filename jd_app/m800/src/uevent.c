#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include "comm.h"
#include "thread.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMIXER_CARD_STR  "hw:UAC1Gadget"
#define PCM_CAPTURE_VOLUME  "PCM Capture Volume"
#define PCM_PLAYBACK_VOLUME  "PCM Playback Volume"

#define CONNECT "USB_STATE=CONNECTED"
#define DISCONNECT  "USB_STATE=DISCONNECTED"
#define CONFIGURED  "USB_STATE=CONFIGURED"
#define STREAM_STATE    "STREAM_STATE"
#define STREAM_STATE_ON   "STREAM_STATE=ON"
#define STREAM_DIRECTION_OUT    "STREAM_DIRECTION=OUT"
#define SET_VOLUME  "SET_VOLUME"
#define SET_MUTE    "SET_MUTE"

static pthread_state_t g_uevent_task_state;
static atomic_int usb_connect = 0;
static atomic_int usb_disconnect = 0;
static atomic_int usb_configured = 0;
static atomic_int stream_out_state = 0;
static atomic_int stream_in_state = 0;

static int last_playback_vol = 0, last_capture_vol = 0;
static const short vol_table[101] = {
// amixer -cUAC1Gadget cget name='PCM Playback Volume'
// numid=3,iface=MIXER,name='PCM Playback Volume'
// ; type=INTEGER,access=rw---R--,values=1,min=0,max=100,step=1
// : values=100
// | dBminmax-min=-100.00dB,max=0.00dB
// amixer -cUAC1Gadget cget name='PCM Capture Volume'
// numid=6,iface=MIXER,name='PCM Capture Volume'
//   ; type=INTEGER,access=rw---R--,values=1,min=0,max=100,step=1
//   : values=0
//   | dBminmax-min=-100.00dB,max=0.00dB
    
    0x9c00,
    0xbbf6,0xc58a,0xcb5f,0xcf93,0xd2dd,0xd591,0xd7dc,0xd9da,0xdb9d,0xdd31,
    0xde9f,0xdfed,0xe121,0xe23f,0xe349,0xe442,0xe52c,0xe608,0xe6d9,0xe79f,
    0xe85c,0xe910,0xe9bc,0xea60,0xeafe,0xeb96,0xec28,0xecb5,0xed3d,0xedc0,
    0xee3f,0xeeba,0xef31,0xefa5,0xf015,0xf083,0xf0ed,0xf154,0xf1b9,0xf21b,
    0xf27b,0xf2d8,0xf334,0xf38d,0xf3e4,0xf439,0xf48d,0xf4de,0xf52e,0xf57d,
    0xf5ca,0xf615,0xf65f,0xf6a7,0xf6ef,0xf735,0xf779,0xf7bd,0xf7ff,0xf840,
    0xf881,0xf8c0,0xf8fe,0xf93b,0xf977,0xf9b2,0xf9ed,0xfa26,0xfa5f,0xfa97,
    0xface,0xfb04,0xfb3a,0xfb6f,0xfba3,0xfbd6,0xfc09,0xfc3b,0xfc6d,0xfc9e,
    0xfcce,0xfcfd,0xfd2d,0xfd5b,0xfd89,0xfdb6,0xfde3,0xfe10,0xfe3c,0xfe67,
    0xfe92,0xfebd,0xfee7,0xff10,0xff39,0xff62,0xff8a,0xffb2,0xffd9,0x0000
};

static int task_init(const int cpu, 
    const int priority, 
    const char *name, 
    pthread_state_t *state,
    thread_func func)
{
    int ret = 0;
    if(name == NULL || state == NULL){
        return -1;
    }

    ret = create_thread(name, cpu, priority, func, state);
    if(ret != 0){
        loge("create task(%s) failed\n", name);
        return -1;
    }

    logi("create task(%s) success\n", name);

    return 0;
}

static void task_destroy(pthread_state_t *state)
{
    if(state == NULL){
        return;
    }
    destroy_thread(state);
}

static void update_usb_connect(int state)
{
    atomic_store(&usb_connect, state);
}
static void update_usb_disconnect(int state)
{
    atomic_store(&usb_disconnect, state);
}
static void update_usb_configured(int state)
{
    atomic_store(&usb_configured, state);
}
static void update_stream_out_state(int state)
{
    atomic_store(&stream_out_state, state);
}
static void update_stream_in_state(int state)
{
    atomic_store(&stream_in_state, state);
}
int check_usb_connect() { return atomic_load(&usb_connect); }
int check_usb_disconnect() { return atomic_load(&usb_disconnect); }
int check_usb_configured() { return atomic_load(&usb_configured); }
int check_stream_out_state() { return atomic_load(&stream_out_state); }
int check_stream_in_state() { return atomic_load(&stream_in_state); }

static bool is_no_need_process_event(const char *event)
{
    bool res = true;
    if(strstr(event, "android_usb") && strstr(event, "USB_STATE"))
        res = false;

    if(strstr(event, "u_audio") && 
        (strstr(event, "STREAM_STATE") 
        || strstr(event, "SET_VOLUME") 
        || strstr(event, "SET_MUTE")))
        res = false;

    return res;
}

static void check_event(char *buf)
{
    const char *t1 = "=";
    const char *t2 = "\n";

    if(strstr(buf, "android_usb")){
        if(strstr(CONNECT, buf)){
            update_usb_disconnect(0);
            update_usb_connect(1);
        }
        else if(strstr(DISCONNECT, buf)){
            update_usb_connect(0);
            update_usb_configured(0);
            update_usb_disconnect(1);
        }
        else if(strstr(CONFIGURED, buf)){
            update_usb_configured(1);
        }
    }
    else if(strstr(buf, "u_audio")){
        if(strstr(buf, STREAM_STATE)){
            int state = strstr(buf, STREAM_STATE_ON) ? 1 : 0;
            int out = strstr(buf, STREAM_DIRECTION_OUT) ? 1 : 0;
            if(out){
                update_stream_out_state(state);
            }
            else{
                update_stream_in_state(state);
            }
        }
        else if(strstr(buf, SET_VOLUME)){
			char *s = strstr(buf, "VOLUME=");
			s = strstr(s, t1);
			char *s1 = strtok(s, t2);
			s1++;
			char *pp;
			short vol = strtol(s1, &pp, 16) & 0xffff;
            int i;
            for (i = 100; i > 0; i--){
                if(vol == vol_table[i]) 
                    break;
            }
            vol = i;

            if(strstr(buf, STREAM_DIRECTION_OUT)){
                alsa_cget(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, &last_playback_vol);
                logd("before cset val: %d\n", last_playback_vol);
                alsa_cset(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, vol);
                alsa_cget(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, &vol);
                logd("after cset val: %d\n", vol);
            }
            else{
                alsa_cget(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, &last_capture_vol);
                logd("before cset val: %d\n", last_capture_vol);
                alsa_cset(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, vol);
                alsa_cget(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, &vol);
                logd("after cset val: %d\n", vol);
            }
        }
        else if(strstr(buf, SET_MUTE)){
			char *s = strstr(buf, "MUTE=");
			s = strstr(s, t1);
			char *s1 = strtok(s, t2);
			s1++;		
			char *pp;
			int mute = strtol(s1, &pp, 16);
			
			if(strstr(buf, STREAM_DIRECTION_OUT)){
                int vol = 0;
                if(mute){
                    alsa_cget(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, &last_playback_vol);
                    logd("before cset val: %d\n", last_playback_vol);
                    alsa_cset(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, 0);
                    alsa_cget(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, &vol);
                    logd("after cset val: %d\n", vol);
                }
                else{
                    alsa_cset(AMIXER_CARD_STR, PCM_PLAYBACK_VOLUME, last_playback_vol);
                }
            }
			else{
                int vol = 0;
                if(mute){
                    alsa_cget(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, &last_capture_vol);
                    logd("before cset val: %d\n", last_capture_vol);
                    alsa_cset(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, 0);
                    alsa_cget(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, &vol);
                    logd("after cset val: %d\n", vol);
                }
                else{
                    alsa_cset(AMIXER_CARD_STR, PCM_CAPTURE_VOLUME, last_capture_vol);
                }
            }
        }
    }
}

static int uevent_task(void *arg)
{
    int sock = -1;
    int err = 0;
    int opt = 2*1024;
    struct sockaddr_nl addr;
    fd_set rfds;
    struct timeval t;
    char buf[1024];

    prctl(PR_SET_NAME, g_uevent_task_state.name);
    logi("---- proc %s start ----\n", g_uevent_task_state.name);

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if(sock < 0){
        loge("create socket for uevent fail\n");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_nl));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;

    err = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
    if(err != 0){
        loge("setsockopt error\n");
        close(sock);
        return -1;
    }

    err = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0){
        loge("bind error(%s)\n", strerror(err));
        close(sock);
        return -1;
    }

    while(g_uevent_task_state.running){
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        t.tv_sec = 1;
        t.tv_usec = 0;
        err = select(sock+1, &rfds, NULL, NULL, &t);
        if(err > 0){
            memset(buf, 0, sizeof(buf));
            err = read(sock, buf, sizeof(buf)-1);
            if(err > 0){
                for(int i = 0; i < err; i++) {
                    if( buf[i] == '\0') {
                        buf[i] = '\n';
                    }
                }
				buf[err] = '\0';
                // logi("buf: %s\n", buf);
                if(is_no_need_process_event(buf))
                    continue;
                check_event(buf);
            }
        }
        delay_ms(100);
    }

    if(sock > 0){
        close(sock);
    }

    logi("---- proc %s stop ----\n", g_uevent_task_state.name);
    return 0;
}

int init_uevent_listen()
{
    int ret = 0;

    ret = task_init(CPU0, 30, "uevent", &g_uevent_task_state, uevent_task);
    if(ret != 0){
        loge("create uevent task error\n");
        return -1;
    }

    return 0;
}

void destroy_event_listen()
{
    task_destroy(&g_uevent_task_state);
}


#ifdef __cplusplus
}
#endif