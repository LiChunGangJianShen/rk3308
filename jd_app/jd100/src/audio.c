#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include "log.h"
#include "API.h"
#include "alsa_api.h"
#include "thread.h"
#include "ringbuffer.h"
#include "signal_handle.h"
#include "udp_server.h"
#include "wav_file.h"

#define CPU_0   0
#define CPU_1   1
#define CPU_2   2
#define CPU_3   3

#define MIC_CARD_NAME   "plughw:es7210,0"
#define LINE_CARD_NAME  "plughw:ti3104,0"
#define UAC_CARD_NAME  "plughw:UAC1Gadget,0"

#define MIC_CAPTURE_CHANNES 8
#define USE_MIC_CHANNELS    7
#define LINE_IN_CHANNELS    2
#define LINE_OUT_CHANNELS   2
#define UAC_IN_CHANNELS 2
#define UAC_OUT_CHANNELS    2

static int g_rate = 0;
static int g_format = 0;
static int g_algo_length = 0;
static int g_period_size = 0;
static int g_buffer_size = 0;
static int g_bytes_of_frame = 0;
static pthread_state_t g_ai_task_state;
static pthread_state_t g_ao_task_state;
static pthread_state_t g_audio_rec_task_state;
static pthread_state_t g_algo_task_state;
static pthread_state_t g_algo1_task_state;
static pthread_state_t g_algo2_task_state;
static int thread_running = TRUE;
static sem_t g_sem_algo[3]={0};

typedef enum{
    rng_idx_farin_line=0,
    rng_idx_farin_usb_1,
    rng_idx_farin_usb_2,
    rng_idx_farin_max,
}rng_idx_far_in_t;
typedef enum{
    rng_idx_mic_1=0,
    rng_idx_mic_2,
    rng_idx_mic_3,
    rng_idx_mic_4,
    rng_idx_mic_5,
    rng_idx_mic_6,
    rng_idx_mic_7,
    rng_idx_ref,//linein_2
    rng_idx_algo_in_max,
}rng_idx_algo_in_t;
typedef enum{
    rng_idx_alg2line=0,
    rng_idx_alg2usb,
    rng_idx_algo_out_max,
}rng_idx_algo_out_t;
typedef enum{
    algo_idx=0,
    algo_idx_1,
    algo_idx_2,
    algo_idx_max,
}rng_idx_algo_t;
typedef enum{
    rec_idx_mic_1=0,
    rec_idx_mic_2,
    rec_idx_mic_3,
    rec_idx_mic_4,
    rec_idx_mic_5,
    rec_idx_mic_6,
    rec_idx_mic_7,
    rec_idx_ref,
    rec_idx_algout,
    rec_idx_max,
}rng_idx_rec_t;
static struct ringbuffer *g_farin_rngbuff[rng_idx_farin_max] = {0};
static struct ringbuffer *g_algo_in_rngbuff[algo_idx_max][rng_idx_algo_in_max] = {0};
static struct ringbuffer *g_algo_out_rngbuff[rng_idx_algo_out_max] = {0};
static struct ringbuffer *g_rec_rngbuff[rec_idx_max] = {0};
static FILE *p_file_rec[rec_idx_max] = {0};
struct my_wave_file_headers st_wavhead[rec_idx_max] = {0};
static int total_size[rec_idx_max] = {0};
typedef enum {
    algo_record_cmd_none,
    algo_record_cmd_start,
    algo_record_cmd_stop,
}algo_record_cmd;
static atomic_int g_record_action = algo_record_cmd_none;

static int get_executable_path( char* processdir,char* processname, int len)
{
    char* path_end;
    if(readlink("/proc/self/exe", processdir,len) <=0)
        return -1;
    path_end = strrchr(processdir,  '/');
    if(path_end == NULL)
        return -1;
    ++path_end;
    strcpy(processname, path_end);
    *path_end = '\0';
    return (size_t)(path_end - processdir);
}

char *algo_version(void)
{
    return JD_MicArray_GetVersion();
}

void init_rng_buff(void)
{
    int i,j;

    for(i = 0; i < rng_idx_farin_max; i++){
        g_farin_rngbuff[i] = rb_create(g_period_size*g_bytes_of_frame*4);
    }
    for(i = 0; i< algo_idx_max; i++){
        for(j = 0; j < rng_idx_algo_in_max; j++){
            g_algo_in_rngbuff[i][j] = rb_create(g_algo_length*g_bytes_of_frame*4);
        }
    }
    for(i = 0; i < rng_idx_algo_out_max; i++){
        // log_dbg("g_algo_out_rngbuff length: %d", g_algo_length*g_bytes_of_frame*4);
        g_algo_out_rngbuff[i] = rb_create(g_algo_length*g_bytes_of_frame*4);
    }
    for(i = 0; i < rec_idx_max; i++){
        g_rec_rngbuff[i] = rb_create(g_rate*g_bytes_of_frame*2);
    }
}

void destroy_rng_buff(void)
{
    int i,j;

    for(i = 0; i < rng_idx_farin_max; i++){
        rb_destroy(g_farin_rngbuff[i]);
    }
    for(i = 0; i< algo_idx_max; i++){
        for(j = 0; j < rng_idx_algo_in_max; j++){
            rb_destroy(g_algo_in_rngbuff[i][j]);
        }
    }
    for(i = 0; i < rng_idx_algo_out_max; i++){
        rb_destroy(g_algo_out_rngbuff[i]);
    }
    for(i = 0; i < rec_idx_max; i++){
        rb_destroy(g_rec_rngbuff[i]);
    }
}

int mono_merge(int a, int b)
{
    int max = 32767;
    int min = -32768;
    if(g_bytes_of_frame > 2){
        max = 65535;
        min = -65536;
    }
    if(a + b > max)
        return max;
    else if(a + b < min)
        return min;
    return a + b;
}

int init_mic_pcm(snd_pcm_t **pcm)
{
    alsa_api_para_t alsa_para;
    int rc = 0;
    memset(&alsa_para, 0, sizeof(alsa_para));
    alsa_para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    alsa_para.block = SND_PCM_NONBLOCK;
    alsa_para.buffer_size = g_buffer_size;
    sprintf(alsa_para.card_name, "%s", MIC_CARD_NAME);
    alsa_para.chn = MIC_CAPTURE_CHANNES;
    switch (g_format)
    {
    case 16:
        alsa_para.format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa_para.format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa_para.format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        break;
    }
    alsa_para.period_size = g_period_size;
    alsa_para.rate = g_rate;
    alsa_para.stream = SND_PCM_STREAM_CAPTURE;
    rc = init_pcm(pcm, alsa_para);
    if(rc != 0){
        log_warn("mic pcm init error");
        return -1;
    }
    return 0;
}

int init_line_in_pcm(snd_pcm_t **pcm)
{
    alsa_api_para_t alsa_para;
    int rc = 0;
    memset(&alsa_para, 0, sizeof(alsa_para));
    alsa_para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    alsa_para.block = SND_PCM_NONBLOCK;
    alsa_para.buffer_size = g_buffer_size;
    sprintf(alsa_para.card_name, "%s", LINE_CARD_NAME);
    alsa_para.chn = LINE_IN_CHANNELS;
    switch (g_format)
    {
    case 16:
        alsa_para.format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa_para.format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa_para.format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        break;
    }
    alsa_para.period_size = g_period_size;
    alsa_para.rate = g_rate;
    alsa_para.stream = SND_PCM_STREAM_CAPTURE;
    rc = init_pcm(pcm, alsa_para);
    if(rc != 0){
        log_warn("line-in pcm init error");
        return -1;
    }
    return 0;
}

int init_usb_in_pcm(snd_pcm_t **pcm)
{
    alsa_api_para_t alsa_para;
    int rc = 0;
    memset(&alsa_para, 0, sizeof(alsa_para));
    alsa_para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    alsa_para.block = SND_PCM_NONBLOCK;
    alsa_para.buffer_size = g_buffer_size;
    sprintf(alsa_para.card_name, "%s", UAC_CARD_NAME);
    alsa_para.chn = UAC_IN_CHANNELS;
    switch (g_format)
    {
    case 16:
        alsa_para.format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa_para.format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa_para.format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        break;
    }
    alsa_para.period_size = g_period_size;
    alsa_para.rate = g_rate;
    alsa_para.stream = SND_PCM_STREAM_CAPTURE;
    rc = init_pcm(pcm, alsa_para);
    if(rc != 0){
        log_warn("usb-in pcm init error");
        return -1;
    }
    return 0;
}

int init_line_out_pcm(snd_pcm_t **pcm)
{
    alsa_api_para_t alsa_para;
    int rc = 0;
    memset(&alsa_para, 0, sizeof(alsa_para));
    alsa_para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    alsa_para.block = SND_PCM_NONBLOCK;
    alsa_para.buffer_size = g_buffer_size;
    sprintf(alsa_para.card_name, "%s", LINE_CARD_NAME);
    alsa_para.chn = LINE_OUT_CHANNELS;
    switch (g_format)
    {
    case 16:
        alsa_para.format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa_para.format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa_para.format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        break;
    }
    alsa_para.period_size = g_period_size;
    alsa_para.rate = g_rate;
    alsa_para.stream = SND_PCM_STREAM_PLAYBACK;
    rc = init_pcm(pcm, alsa_para);
    if(rc != 0){
        log_warn("line-out pcm init error");
        return -1;
    }
    return 0;
}

int init_usb_out_pcm(snd_pcm_t **pcm)
{
    alsa_api_para_t alsa_para;
    int rc = 0;
    memset(&alsa_para, 0, sizeof(alsa_para));
    alsa_para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    alsa_para.block = SND_PCM_NONBLOCK;
    alsa_para.buffer_size = g_buffer_size;
    sprintf(alsa_para.card_name, "%s", UAC_CARD_NAME);
    alsa_para.chn = UAC_OUT_CHANNELS;
    switch (g_format)
    {
    case 16:
        alsa_para.format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa_para.format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa_para.format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        break;
    }
    alsa_para.period_size = g_period_size;
    alsa_para.rate = g_rate;
    alsa_para.stream = SND_PCM_STREAM_PLAYBACK;
    rc = init_pcm(pcm, alsa_para);
    if(rc != 0){
        log_warn("usb-out pcm init error");
        return -1;
    }
    return 0;
}

int ai_task(void *arg)
{
    prctl(PR_SET_NAME, "ai_task");
    log_info("ai task start");

    int rc = 0;
    int chn_idx = 0;
    int i,j,k;
    snd_pcm_t *pcm_mic = NULL;
    snd_pcm_t *pcm_line_in = NULL;
    snd_pcm_t *pcm_usb_in = NULL;
    snd_pcm_sframes_t avail_frames = 0;
    snd_pcm_sframes_t mic_frames = 0;

    char *mic_buff = (char *)malloc(g_period_size*g_bytes_of_frame*MIC_CAPTURE_CHANNES);
    char *line_in_buff = (char *)malloc(g_period_size*g_bytes_of_frame*LINE_IN_CHANNELS);
    char *usb_in_buff = (char *)malloc(g_period_size*g_bytes_of_frame*UAC_IN_CHANNELS);
    char *tmp_buff = (char *)malloc(g_period_size*g_bytes_of_frame);

    init_mic_pcm(&pcm_mic);
    init_line_in_pcm(&pcm_line_in);
    init_usb_in_pcm(&pcm_usb_in);

    snd_pcm_start(pcm_mic);
    snd_pcm_start(pcm_line_in);
    snd_pcm_start(pcm_usb_in);
    while(g_ai_task_state.running){
        if(pcm_mic){
            avail_frames = snd_pcm_avail(pcm_mic);
            if(avail_frames >= g_period_size){
                rc = pcm_in(pcm_mic, mic_buff, g_period_size, "mic-in");
                if(rc > 0){
                    for(chn_idx = 0; chn_idx < MIC_CAPTURE_CHANNES; chn_idx++){
                        if(chn_idx > rng_idx_mic_7)//只有7个mic
                            break;
                        memset(tmp_buff, 0, g_period_size*g_bytes_of_frame);
                        for(i = 0, j = 0; i < g_period_size; i++, j += g_bytes_of_frame){
                            ((short *)tmp_buff)[i] = ((short *)mic_buff)[j + chn_idx];
                        }
                        for(k = 0; k < algo_idx_max; k++){
                            if(rb_get_space_free(g_algo_in_rngbuff[k][chn_idx+rng_idx_mic_1]) < g_period_size*g_bytes_of_frame){
                                rb_discard(g_algo_in_rngbuff[k][chn_idx+rng_idx_mic_1], g_period_size*g_bytes_of_frame);
                            }
                            rb_write(g_algo_in_rngbuff[k][chn_idx+rng_idx_mic_1], tmp_buff, g_period_size*g_bytes_of_frame);
                        }
                    }
                }
                mic_frames += avail_frames;
            }
            else if(avail_frames < 0){
                check_pcm_state(pcm_mic, avail_frames, "mic-in");
            }
        }
        if(pcm_line_in){
            avail_frames = snd_pcm_avail(pcm_line_in);
            if(avail_frames >= g_period_size){
                rc = pcm_in(pcm_line_in, line_in_buff, g_period_size, "line-in");
                if(rc > 0){
                    for(chn_idx = 0; chn_idx < LINE_IN_CHANNELS; chn_idx++){
                        //chn-1 ref
                        //chn-2 line-in
                        memset(tmp_buff, 0, g_period_size*g_bytes_of_frame);
                        for(i = 0, j = 0; i < g_period_size; i++, j += g_bytes_of_frame){
                            ((short *)tmp_buff)[i] = ((short *)line_in_buff)[j + chn_idx];
                        }
                        if(chn_idx == 0){
                            for(k = 0; k < algo_idx_max; k++){
                                if(rb_get_space_free(g_algo_in_rngbuff[k][rng_idx_ref]) < g_period_size*g_bytes_of_frame){
                                    rb_discard(g_algo_in_rngbuff[k][rng_idx_ref], g_period_size*g_bytes_of_frame);
                                }
                                rb_write(g_algo_in_rngbuff[k][rng_idx_ref], tmp_buff, g_period_size*g_bytes_of_frame);
                            }
                        }
                        else if(chn_idx == 1){
                            if(rb_get_space_free(g_farin_rngbuff[rng_idx_farin_line]) < g_period_size*g_bytes_of_frame){
                                rb_discard(g_farin_rngbuff[rng_idx_farin_line], g_period_size*g_bytes_of_frame);
                            }
                            rb_write(g_farin_rngbuff[rng_idx_farin_line], tmp_buff, g_period_size*g_bytes_of_frame);
                        }
                    }
                }
            }
            else if(avail_frames < 0){
                check_pcm_state(pcm_line_in, avail_frames, "line-in");
            }
        }
        if(pcm_usb_in){
            avail_frames = snd_pcm_avail(pcm_usb_in);
            if(avail_frames >= g_period_size){
                rc = pcm_in(pcm_usb_in, usb_in_buff, g_period_size, "usb-in");
                if(rc > 0){
                    for(chn_idx = 0; chn_idx < UAC_IN_CHANNELS; chn_idx++){
                        memset(tmp_buff, 0, g_period_size*g_bytes_of_frame);
                        for(i = 0, j = 0; i < g_period_size; i++, j += g_bytes_of_frame){
                            ((short *)tmp_buff)[i] = ((short *)usb_in_buff)[j + chn_idx];
                        }
                        if(rb_get_space_free(g_farin_rngbuff[chn_idx+rng_idx_farin_usb_1]) < g_period_size*g_bytes_of_frame){
                            rb_discard(g_farin_rngbuff[chn_idx+rng_idx_farin_usb_1], g_period_size*g_bytes_of_frame);
                        }
                        rb_write(g_farin_rngbuff[chn_idx+rng_idx_farin_usb_1], tmp_buff, g_period_size*g_bytes_of_frame);
                    }
                }
            }
            else if(avail_frames < 0){
                check_pcm_state(pcm_usb_in, avail_frames, "usb-in");
            }
        }

        if(mic_frames >= g_algo_length){
            mic_frames -= g_algo_length;
            for(i = 0; i < algo_idx_max; i++){
                sem_post(&g_sem_algo[i]);
            }
        }

        usleep(100);
    }

    for(i = 0; i < algo_idx_max; i++){
        sem_post(&g_sem_algo[i]);
    }

    if(mic_buff){
        free(mic_buff);
        mic_buff = NULL;
    }
    if(line_in_buff){
        free(line_in_buff);
        line_in_buff = NULL;
    }
    if(tmp_buff){
        free(tmp_buff);
        tmp_buff = NULL;
    }

    if(pcm_mic){
        snd_pcm_drop(pcm_mic);
        snd_pcm_close(pcm_mic);
    }
    if(pcm_line_in){
        snd_pcm_drop(pcm_line_in);
        snd_pcm_close(pcm_line_in);
    }
    if(pcm_usb_in){
        snd_pcm_drop(pcm_usb_in);
        snd_pcm_close(pcm_usb_in);
    }

    log_info("ai task stop");
    return 0;
}

int ao_task(void *arg)
{
    prctl(PR_SET_NAME, "ao_task");
    log_info("ao task start");

    int chn_idx = 0;
    int i,j;
    snd_pcm_t *pcm_line_out = NULL;
    snd_pcm_t *pcm_usb_out = NULL;
    snd_pcm_sframes_t avail_frames = 0;

    char *tmp_buff = (char *)malloc(g_period_size*g_bytes_of_frame);
    char *linein_buff = (char *)malloc(g_period_size*g_bytes_of_frame);
    char *usb_in1_buff = (char *)malloc(g_period_size*g_bytes_of_frame);
    char *usb_in2_buff = (char *)malloc(g_period_size*g_bytes_of_frame);
    char *algout_buff = (char *)malloc(g_period_size*g_bytes_of_frame);
    char *line_out_buff = (char *)malloc(g_period_size*g_bytes_of_frame*LINE_OUT_CHANNELS);
    char *usb_out_buff = (char *)malloc(g_period_size*g_bytes_of_frame*UAC_OUT_CHANNELS);
    
    init_line_out_pcm(&pcm_line_out);
    snd_pcm_start(pcm_line_out);
    init_usb_out_pcm(&pcm_usb_out);
    snd_pcm_start(pcm_usb_out);

    while(g_ao_task_state.running){
        if(pcm_line_out){
            avail_frames = snd_pcm_avail(pcm_line_out);
            if(avail_frames >= g_period_size){
                // log_dbg("line out!!!!!!");
                if(rb_get_space_used(g_algo_out_rngbuff[rng_idx_alg2line]) >= g_period_size*g_bytes_of_frame){
                    rb_read(g_algo_out_rngbuff[rng_idx_alg2line], algout_buff, g_period_size*g_bytes_of_frame);
                    // log_dbg("line out get data !!!!!!");
                }
                if(rb_get_space_used(g_farin_rngbuff[rng_idx_farin_line]) >= g_period_size*g_bytes_of_frame){
                    rb_read(g_farin_rngbuff[rng_idx_farin_line], linein_buff, g_period_size*g_bytes_of_frame);
                }
                if(rb_get_space_used(g_farin_rngbuff[rng_idx_farin_usb_1]) >= g_period_size*g_bytes_of_frame){
                    rb_read(g_farin_rngbuff[rng_idx_farin_usb_1], usb_in1_buff, g_period_size*g_bytes_of_frame);
                }
                if(rb_get_space_used(g_farin_rngbuff[rng_idx_farin_usb_2]) >= g_period_size*g_bytes_of_frame){
                    rb_read(g_farin_rngbuff[rng_idx_farin_usb_2], usb_in2_buff, g_period_size*g_bytes_of_frame);
                }

                for(chn_idx = 0; chn_idx < LINE_OUT_CHANNELS; chn_idx++){
                    //line-out-1 aecout
                    //line-out-2 spkout
                    memset(tmp_buff, 0, g_period_size*g_bytes_of_frame);
                    for(i = 0, j = 0; i < g_period_size; i++, j += g_bytes_of_frame){
                        if(chn_idx == 0){
                            ((short *)line_out_buff)[i + chn_idx] = ((short *)algout_buff)[i];
                        }
                        else if(chn_idx == 1){
                            ((short *)tmp_buff)[i] = mono_merge(mono_merge(((short *)usb_in1_buff)[i], ((short *)usb_in1_buff)[i]), ((short *)linein_buff)[i]);
                            ((short *)line_out_buff)[i + chn_idx] = ((short *)tmp_buff)[i];
                        }
                    }
                }
                pcm_out(pcm_line_out, line_out_buff, g_period_size, "line-out");
            }
            else if(avail_frames < 0){
                check_pcm_state(pcm_line_out, avail_frames, "line-out");
            }
        }
        if(pcm_usb_out){
            avail_frames = snd_pcm_avail(pcm_usb_out);
            if(avail_frames >= g_period_size){
                log_dbg("usb out!!!!!!");
                if(rb_get_space_used(g_algo_out_rngbuff[chn_idx+rng_idx_alg2usb]) >= g_period_size*g_bytes_of_frame){
                    rb_read(g_algo_out_rngbuff[chn_idx+rng_idx_alg2usb], algout_buff, g_period_size*g_bytes_of_frame);
                    log_dbg("usb out get data !!!!!!");
                }
                for(chn_idx = 0; chn_idx < UAC_OUT_CHANNELS; chn_idx++){
                    memset(tmp_buff, 0, g_period_size*g_bytes_of_frame);
                    for(i = 0, j = 0; i < g_period_size; i++, j += g_bytes_of_frame){
                        ((short *)usb_out_buff)[i + chn_idx] = ((short *)algout_buff)[i];
                    }
                }
                pcm_out(pcm_usb_out, usb_out_buff, g_period_size, "uac-out");
            }
            else if(avail_frames < 0){
                check_pcm_state(pcm_usb_out, avail_frames, "uac-out");
            }
        }
        usleep(100);
    }

    if(tmp_buff){
        free(tmp_buff);
        tmp_buff = NULL;
    }
    if(linein_buff){
        free(linein_buff);
        linein_buff = NULL;
    }
    if(usb_in1_buff){
        free(usb_in1_buff);
        usb_in1_buff = NULL;
    }
    if(usb_in2_buff){
        free(usb_in2_buff);
        usb_in2_buff = NULL;
    }
    if(algout_buff){
        free(algout_buff);
        algout_buff = NULL;
    }
    if(line_out_buff){
        free(line_out_buff);
        line_out_buff = NULL;
    }
    if(usb_out_buff){
        free(usb_out_buff);
        usb_out_buff = NULL;
    }

    if(pcm_line_out){
        snd_pcm_drain(pcm_line_out);
        snd_pcm_close(pcm_line_out);
    }
    if(pcm_usb_out){
        snd_pcm_drain(pcm_usb_out);
        snd_pcm_close(pcm_usb_out);
    }

    log_info("ao task stop");
    return 0;
}

int ai_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_ai_task_state, 0, sizeof(g_ai_task_state));
    ret  = create_thread("ai_task", cpu, high_priority, ai_task, &g_ai_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "ai_task");
    }

    return ret;
}

void ai_thread_destroy(void)
{
    destroy_thread(&g_ai_task_state);
}

int ao_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_ao_task_state, 0, sizeof(g_ao_task_state));
    ret  = create_thread("ao_task", cpu, high_priority, ao_task, &g_ao_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "ao_task");
    }

    return ret;
}

void ao_thread_destroy(void)
{
    destroy_thread(&g_ao_task_state);
}

int algo_task(void *arg)
{
    prctl(PR_SET_NAME, "algo_task");
    log_info("algo task start");
    int chn_idx = 0;
    int i,j;

    char *mic_buff = (char *)malloc(g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
    char *ref_buff = (char *)malloc(g_algo_length*g_bytes_of_frame);
    char *out_buff = (char *)malloc(g_algo_length*g_bytes_of_frame);

    while(g_algo_task_state.running){
        if(rb_get_space_used(g_algo_in_rngbuff[algo_idx][rng_idx_mic_1]) < g_algo_length*g_bytes_of_frame ||
            rb_get_space_used(g_algo_in_rngbuff[algo_idx][rng_idx_ref]) < g_algo_length*g_bytes_of_frame){
            sem_wait(&g_sem_algo[0]);
            continue;
        }
        
        rb_read(g_algo_in_rngbuff[algo_idx][rng_idx_mic_1], mic_buff, g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
        rb_read(g_algo_in_rngbuff[algo_idx][rng_idx_ref], ref_buff, g_algo_length*g_bytes_of_frame);
        JD_MicArray_Process1((short *)mic_buff, (short *)out_buff, (short *)ref_buff);
        if(rb_get_space_free(g_algo_out_rngbuff[rng_idx_alg2line]) < g_algo_length*g_bytes_of_frame)
            rb_discard(g_algo_out_rngbuff[rng_idx_alg2line], g_algo_length*g_bytes_of_frame);
        rb_write(g_algo_out_rngbuff[rng_idx_alg2line], out_buff, g_algo_length*g_bytes_of_frame);
        if(rb_get_space_free(g_algo_out_rngbuff[rng_idx_alg2usb]) < g_algo_length*g_bytes_of_frame)
            rb_discard(g_algo_out_rngbuff[rng_idx_alg2usb], g_algo_length*g_bytes_of_frame);
        rb_write(g_algo_out_rngbuff[rng_idx_alg2usb], out_buff, g_algo_length*g_bytes_of_frame);

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            if(rb_get_space_free(g_rec_rngbuff[rec_idx_ref]) < g_algo_length*g_bytes_of_frame)
                rb_discard(g_rec_rngbuff[rec_idx_ref], g_algo_length*g_bytes_of_frame);
            rb_write(g_rec_rngbuff[rec_idx_ref], ref_buff, g_algo_length*g_bytes_of_frame);
            if(rb_get_space_free(g_rec_rngbuff[rec_idx_algout]) < g_algo_length*g_bytes_of_frame)
                rb_discard(g_rec_rngbuff[rec_idx_algout], g_algo_length*g_bytes_of_frame);
            rb_write(g_rec_rngbuff[rec_idx_algout], out_buff, g_algo_length*g_bytes_of_frame);
            for(chn_idx = 0; chn_idx < USE_MIC_CHANNELS; chn_idx++){
                memset(ref_buff, 0, g_algo_length*g_bytes_of_frame);
                for(i = 0, j = 0; i < g_algo_length; i++, j+=g_bytes_of_frame){
                    ((short *)ref_buff)[i] = ((short *)mic_buff)[j + chn_idx];
                }
            }
        }

        usleep(100);
    }

    if(mic_buff){
        free(mic_buff);
        mic_buff = NULL;
    }
    if(ref_buff){
        free(ref_buff);
        ref_buff = NULL;
    }
    if(out_buff){
        free(out_buff);
        out_buff = NULL;
    }

    log_info("algo task stop");
    return 0;
}

int algo1_task(void *arg)
{
    prctl(PR_SET_NAME, "algo1_task");
    log_info("algo1 task start");

    char *mic_buff = (char *)malloc(g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
    char *ref_buff = (char *)malloc(g_algo_length*g_bytes_of_frame);

    while(g_algo1_task_state.running){
        if(rb_get_space_used(g_algo_in_rngbuff[algo_idx_1][rng_idx_mic_1]) < g_algo_length*g_bytes_of_frame ||
            rb_get_space_used(g_algo_in_rngbuff[algo_idx_1][rng_idx_ref]) < g_algo_length*g_bytes_of_frame){
            sem_wait(&g_sem_algo[0]);
            continue;
        }
        
        rb_read(g_algo_in_rngbuff[algo_idx_1][rng_idx_mic_1], mic_buff, g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
        rb_read(g_algo_in_rngbuff[algo_idx_1][rng_idx_ref], ref_buff, g_algo_length*g_bytes_of_frame);
        JD_MicArray_Process2((short *)mic_buff, (short *)ref_buff);

        usleep(100);
    }

    if(mic_buff){
        free(mic_buff);
        mic_buff = NULL;
    }
    if(ref_buff){
        free(ref_buff);
        ref_buff = NULL;
    }

    log_info("algo1 task stop");
    return 0;
}

int algo2_task(void *arg)
{
    prctl(PR_SET_NAME, "algo2_task");
    log_info("algo2 task start");

    char *mic_buff = (char *)malloc(g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
    char *ref_buff = (char *)malloc(g_algo_length*g_bytes_of_frame);

    while(g_algo2_task_state.running){
        if(rb_get_space_used(g_algo_in_rngbuff[algo_idx_2][rng_idx_mic_1]) < g_algo_length*g_bytes_of_frame ||
            rb_get_space_used(g_algo_in_rngbuff[algo_idx_2][rng_idx_ref]) < g_algo_length*g_bytes_of_frame){
            sem_wait(&g_sem_algo[2]);
            continue;
        }
        
        rb_read(g_algo_in_rngbuff[algo_idx_2][rng_idx_mic_1], mic_buff, g_algo_length*g_bytes_of_frame*USE_MIC_CHANNELS);
        rb_read(g_algo_in_rngbuff[algo_idx_2][rng_idx_ref], ref_buff, g_algo_length*g_bytes_of_frame);
        JD_MicArray_Process2((short *)mic_buff, (short *)ref_buff);

        usleep(100);
    }

    if(mic_buff){
        free(mic_buff);
        mic_buff = NULL;
    }
    if(ref_buff){
        free(ref_buff);
        ref_buff = NULL;
    }

    log_info("algo2 task stop");
    return 0;
}

int algo_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_algo_task_state, 0, sizeof(g_algo_task_state));
    ret  = create_thread("algo_task", cpu, high_priority, algo_task, &g_algo_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "algo_task");
    }

    return ret;
}

void algo_thread_destroy(void)
{
    destroy_thread(&g_algo_task_state);
}

int algo1_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_algo1_task_state, 0, sizeof(g_algo1_task_state));
    ret  = create_thread("algo1_task", cpu, high_priority, algo1_task, &g_algo1_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "algo1_task");
    }

    return ret;
}

void algo1_thread_destroy(void)
{
    destroy_thread(&g_algo1_task_state);
}

int algo2_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_algo2_task_state, 0, sizeof(g_algo2_task_state));
    ret  = create_thread("algo2_task", cpu, high_priority, algo2_task, &g_algo2_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "algo2_task");
    }

    return ret;
}

void algo2_thread_destroy(void)
{
    destroy_thread(&g_algo2_task_state);
}

//RECORD
#define REC_RNGBUFF_SIZE SAMPLE_RATE*4    //4s
#define UDP_SERVER_PORT    (6000)
#define RECORD_CMD_START   "record-cmd-start"
#define RECORD_CMD_STOP    "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"
#define MAX_REC_TIME    60   //s

static void rec_start(void)
{
	int i;
	char path[256] = {0};

	for(i = 0; i < rec_idx_max; i++) {
        if(i < rec_idx_ref){
            snprintf(path, sizeof(path), "/data/mic_%d.wav", i+rec_idx_mic_1);
        }
        else if(i <= rec_idx_ref){
            snprintf(path, sizeof(path), "/data/ref.wav");
        }
        else if(i == rec_idx_algout){
            snprintf(path, sizeof(path), "/data/out.wav");
        }

		if(!p_file_rec[i]) {
			p_file_rec[i] = fopen(path, "w+");
            wav_start_write(p_file_rec[i], &st_wavhead[i], g_format, 1, g_rate);
		}
	}
}

static void rec_stop(void)
{
	int i;

    for(i = 0; i < rec_idx_max; i++) {
        if(p_file_rec[i]){
            wav_stop_write(p_file_rec[i], &st_wavhead[i], total_size[i]);
            fclose(p_file_rec[i]);
            p_file_rec[i] = NULL;
        }
    }
}

static unsigned long get_sys_ms(void) {
    struct timeval tv;
    unsigned long t;

    gettimeofday(&tv, NULL);
    t = (unsigned long)tv.tv_sec * 1000 + (unsigned long)tv.tv_usec / 1000;

    return t;
}

int audio_rec_task(void *arg)
{
    prctl(PR_SET_NAME, "audio_rec_task");
    log_info("audio rec task start");

    int i, wcnt,ret,poll_ms;
    udp_server* recv_udp = NULL;
    char buf[1024] = {0};
    struct sockaddr_in client_addr;
    unsigned long cur_time, rest_time;
    static unsigned long start_record_time = 0;
    static unsigned long expect_time = 0;
    char *tmpbuff = (char *)malloc(g_rate*g_bytes_of_frame);

    recv_udp = udp_server_init(UDP_SERVER_PORT);
    if(!recv_udp){
        log_warn("record recv udp init error");
        return -1;
    }
    poll_ms = 1000; //select超时时间
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    while(g_audio_rec_task_state.running){
        ret = udp_server_recv(recv_udp, &client_addr, buf, sizeof(buf), poll_ms);
        if(ret > 0){
            buf[sizeof(buf) - 1] = '\0';
            log_dbg("recv cmmd: %s", buf);
            if (strstr(buf, RECORD_CMD_START)) {
                start_record_time = get_sys_ms();
                cur_time = start_record_time;
                rec_start();
                for(int i = 0; i < rec_idx_max; i++){
                    rb_discard(g_rec_rngbuff[i], rb_get_space_used(g_rec_rngbuff[i]));
                }
                atomic_store(&g_record_action, algo_record_cmd_start);
                udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                log_dbg("send cmd: %s, ret:%d", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STOP)) {
                atomic_store(&g_record_action, algo_record_cmd_stop);
                rec_stop();
                ret = udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                log_dbg("send cmd: %s, ret:%d", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STATUS)) {
                if(atomic_load(&g_record_action) == algo_record_cmd_start)  {
                    cur_time = get_sys_ms();
                    rest_time = expect_time - (cur_time - start_record_time)/1000;
                    log_dbg("rest_time=%ld s", rest_time);
                    snprintf(buf, sizeof(buf), "%s=%s %lds", RECORD_CMD_STATUS, "going", rest_time);
                    udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                    log_dbg("send cmd: %s, ret:%d", buf, ret);
                    if(rest_time <= 0){
                        atomic_store(&g_record_action, algo_record_cmd_stop);
                        snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "finish");
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        log_dbg("send cmd: %s, ret:%d", buf, ret);
                        rec_stop();
                    }
                }
            } else if (strstr(buf, "sec-")) {
                int i = 0;
                while(buf[i] != '-')
                    i++;
                i++;
                expect_time = atoi(buf+i);
                if(expect_time > MAX_REC_TIME)
                    expect_time = MAX_REC_TIME;
                log_dbg("expect_time:%ld s", expect_time);
            }
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            for(i = 0; i < rec_idx_max; i++){
                if(rb_get_space_used(g_rec_rngbuff[i]) >= sizeof(tmpbuff)){
                    rb_read(g_rec_rngbuff[i], tmpbuff, sizeof(tmpbuff));
                    if(p_file_rec[i]){
                        wcnt = fwrite(tmpbuff, 1, sizeof(tmpbuff), p_file_rec[i]);
                        total_size[i] += wcnt;
                    }
                }
            }
        }
        if(atomic_load(&g_record_action) == algo_record_cmd_stop){
            atomic_store(&g_record_action, algo_record_cmd_none);
            for(i = 0; i < rec_idx_max; i++){
                if(rb_get_space_used(g_rec_rngbuff[i]) > 0){
                    rb_read(g_rec_rngbuff[i], tmpbuff, sizeof(tmpbuff));
                    if(p_file_rec[i]){
                        wcnt = fwrite(tmpbuff, 1, sizeof(tmpbuff), p_file_rec[i]);
                        total_size[i] += wcnt;
                    }
                }
            }
        }
        usleep(100);
    }

    if(tmpbuff){
        free(tmpbuff);
        tmpbuff = NULL;
    }
    log_info("audio rec task stop");
    return 0;
}

int audio_rec_thread_start(int cpu)
{
    int ret = 0;
    int high_priority = FALSE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)", cpu);
        return -1;
    }

    memset(&g_audio_rec_task_state, 0, sizeof(g_audio_rec_task_state));
    ret  = create_thread("audio_rec_task", cpu, high_priority, audio_rec_task, &g_audio_rec_task_state);
    if(ret != 0){
        log_warn("create %s thread failed", "audio_rec_task");
    }

    return ret;
}

void audio_rec_thread_destroy(void)
{
    destroy_thread(&g_audio_rec_task_state);
}

int check_rate(int rate)
{
    int err = 0;
    switch (rate)
    {
    case 22050:
    case 44100:
    case 16000:
    case 32000:
    case 48000:
        break;
    default:
        err = -1;
        break;
    }

    return err;
}

int check_format(int format)
{
    int err = 0;
    switch (format)
    {
    case 16:
    // case 24:
    case 32:
        break;
    default:
        err = -1;
        break;
    }

    return err;
}

int audio_start(int rate, int format, int algo_period_length)
{
    char info[256]={0};
    char path[PATH_MAX] = {0};
    char process_name[32] = {0};
    get_executable_path(path, process_name, PATH_MAX);
    log_info("%s start", process_name);
    signal_hanler_init(process_name, &thread_running);

    sprintf(info, "\nrate: %d\nformat: %d\nalgo_length: %d", rate, format, algo_period_length);
    log_dbg("%s", info);
    g_rate = rate;
    g_format = format;
    if(check_rate(g_rate) < 0){
        log_warn("invalid rate\n\t22050\n\t44100\n\t16000\n\t32000\n\t48000\nThe sample rate above will be supported");
        thread_running = FALSE;
        return -1;
    }
    if(check_format(g_format) < 0){
        log_warn("invalid format\n\t16\n\t32\nThe sample format above will be supported");
        thread_running = FALSE;
        return -1;
    }
    g_algo_length = algo_period_length;
    if(g_rate == 22050)
        g_period_size = 220;
    else if(g_rate == 22050)
        g_period_size = 440;
    else
        g_period_size = (g_rate*10)/1000;
    log_dbg("g_period_size: %d", g_period_size);
    g_buffer_size = 4*g_period_size;
    g_bytes_of_frame = (g_format/8);

    for(int i = 0; i < 3; i++){
        sem_init(&g_sem_algo[i], 0, 0);
    }
    init_rng_buff();
    ai_thread_start(CPU_0);
    ao_thread_start(CPU_0);
    audio_rec_thread_start(CPU_0);
    algo_thread_start(CPU_1);
    algo1_thread_start(CPU_2);
    algo2_thread_start(CPU_3);

    while(thread_running){
        sleep(1);
    }

    algo_thread_destroy();
    algo1_thread_destroy();
    algo2_thread_destroy();
    audio_rec_thread_destroy();
    ao_thread_destroy();
    ai_thread_destroy();
    for(int i = 0; i < 3; i++){
        sem_destroy(&g_sem_algo[i]);
    }

    signal_hanler_exit();
    destroy_rng_buff();

    return 0;
}