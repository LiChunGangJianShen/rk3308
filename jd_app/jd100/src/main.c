#include "API.h"
#include "thread.h"
#include "build_time.h"
#include "app_version.h"
#include "alsa_api.h"
#include "log.h"
#include "ringbuffer.h"
#include "udp_server.h"
#include "signal_handle.h"
#include "rkgpio.h"
#include "wav_file.h"
#include "rk3308_soc.h"
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

#define ENABLED_ALGO    0

#define CPU0    0
#define CPU1    1
#define CPU2    2
#define CPU3    3

#define SAMPLE_RATE    22050
#define PERIOD_SIZE 220
#define BUFFER_SIZE 4*PERIOD_SIZE
#define ALGO_PERIOD_SIZE    256
#define MIC_CHN 8
#define LINEIN_CHN 2
#define LINEOUT_CHN 2
#define UAC_CHN 2

#define MIC_CARD_ID "es7210"
#define LINE_CARD_ID "ti3104"
#define UAC_CARD_ID "UAC1Gadget"

static pthread_state_t state_task_mic;
static pthread_state_t state_task1;
static pthread_state_t state_task2;
static pthread_state_t state_task3;
static pthread_state_t state_task_rec;
static pthread_state_t state_task_other_in_and_out;

enum{
    algo_1=0,
    algo_2,
    algo_3,
    algo_max,
};
enum{
    mic_1=0,
    mic_2,
    mic_3,
    mic_4,
    mic_5,
    mic_6,
    mic_7,
    mic_max,
};
enum{
    rec_mic1=0,
    rec_mic2,
    rec_mic3,
    rec_mic4,
    rec_mic5,
    rec_mic6,
    rec_mic7,
    rec_ref,
    rec_out,
    rec_max,
};
static ringbuffer_t *g_mic_rngbuff[algo_max] = {0};
static ringbuffer_t *g_ref_rngbuff[algo_max] = {0};
static ringbuffer_t *g_algo_out_rngbuff = 0;
static sem_t g_sem_alg[algo_max]={0};
//RECORD
#define REC_RNGBUFF_SIZE SAMPLE_RATE*4  //4s
#define UDP_SERVER_PORT    (6000)
#define RECORD_CMD_START   "record-cmd-start"
#define RECORD_CMD_STOP    "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"
#define MAX_REC_TIME    90   //s
#define REC_CHN (MIC_CHN-1)+1+1 //7mic+1ref+1out

typedef enum {
    algo_record_cmd_none,
    algo_record_cmd_start,
    algo_record_cmd_stop,
}algo_record_cmd;
static ringbuffer_t *g_rec_rngbuff = 0;
static FILE *p_file_rec = NULL;
struct my_wave_file_headers st_wavhead;
static unsigned long total_size = 0;
static atomic_int g_record_action = algo_record_cmd_none;

/**************************************************************************/
static unsigned long get_sys_ms(void) {
    struct timeval tv;
    unsigned long t;

    gettimeofday(&tv, NULL);
    t = (unsigned long)tv.tv_sec * 1000 + (unsigned long)tv.tv_usec / 1000;

    return t;
}
/**************************************************************************/
//led
static void init_led(void)
{
    rk_set_gpio_export(1, 'A', 7);
    rk_set_gpio_direction_out(1, 'A', 7);
    rk_set_gpio_value(1, 'A', 7, 1);
}
static void update_led_sta(int sta)
{
    rk_set_gpio_value(1, 'A', 7, sta);
}
static void destroy_led(void)
{
    rk_set_gpio_value(1, 'A', 7, 0);
    rk_set_gpio_unexport(1, 'A', 7);
}
/**************************************************************************/
static int task_mic(void *arg)
{
    short buff[PERIOD_SIZE][MIC_CHN] = {0};
    alsa_para_t para;
    snd_pcm_t *pcm = NULL;
    snd_pcm_sframes_t avail_frames = 0;
    int rc = 0, i, get_frames=0;

    para.rate = SAMPLE_RATE;
    para.chn = MIC_CHN;
    para.period_size = PERIOD_SIZE;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.format = SND_PCM_FORMAT_S16_LE;
    para.stream = SND_PCM_STREAM_CAPTURE;
    para.mode = SND_PCM_NONBLOCK;
    sprintf(para.card_name, "plughw:%s,0", MIC_CARD_ID);
    rc = init_pcm(&pcm, &para);
    if(rc < 0){
        log_err("mic init pcm fail");
        return -1;
    }
    snd_pcm_start(pcm);
    if(PERIOD_SIZE != para.period_size){
        char info[256]={0};
        sprintf(info, "mic: PERIOD_SIZE=%d, para.period_size=%d", PERIOD_SIZE, (int)para.period_size);
        log_warn("%s", info);
    }

    while(state_task3.running){
        avail_frames = snd_pcm_avail(pcm);
        if(avail_frames >= PERIOD_SIZE){
            rc = pcm_in(pcm, buff, para.period_size, "mic-in");
            if(rc > 0){
                for(i = 0; i < algo_max; i++){
                    if(rb_get_space_free(g_mic_rngbuff[i]) < sizeof(buff)){
                        rb_discard(g_mic_rngbuff[i], sizeof(buff));
                    }
                    rb_write(g_mic_rngbuff[i], buff, sizeof(buff));
                }
                get_frames += rc;
            }
        }
        else if(avail_frames < 0){
            check_pcm_state(pcm, avail_frames, "mic-in");
        }

        if(get_frames >= ALGO_PERIOD_SIZE){
            get_frames -= ALGO_PERIOD_SIZE;
            for(i = 0; i < algo_max; i++){
                sem_post(&g_sem_alg[i]);
            }
        }

        usleep(100);
    }

    destroy_pcm(pcm);

    return 0;
}
static int init_task_mic(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task_mic, 0, sizeof(state_task_mic));
    ret  = create_thread("task_mic", cpu, high_priority, task_mic, &state_task_mic);
    if(ret != 0){
        log_warn("create %s thread failed\n", "task_mic");
    }

    return ret;
}
static void exit_task_mic(void)
{
    destroy_thread(&state_task_mic);
}
/**************************************************************************/
static int task_other_in_and_out(void *arg)
{
    short linein_buff[PERIOD_SIZE][LINEIN_CHN] = {0};
    short lineout_buff[PERIOD_SIZE][LINEOUT_CHN] = {0};
    short uacin_buff[PERIOD_SIZE][UAC_CHN] = {0};
    short uacout_buff[PERIOD_SIZE][UAC_CHN] = {0};
    short tmpbuff[PERIOD_SIZE] = {0};
    alsa_para_t para;
    snd_pcm_t *pcm_linein = NULL;
    snd_pcm_t *pcm_lineout = NULL;
    snd_pcm_t *pcm_uacin = NULL;
    snd_pcm_t *pcm_uacout = NULL;
    snd_pcm_sframes_t avail_frames = 0;
    int rc = 0, i;

    para.rate = SAMPLE_RATE;
    para.chn = LINEIN_CHN;
    para.period_size = PERIOD_SIZE;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.format = SND_PCM_FORMAT_S16_LE;
    para.stream = SND_PCM_STREAM_CAPTURE;
    para.mode = SND_PCM_NONBLOCK;
    sprintf(para.card_name, "plughw:%s,0", LINE_CARD_ID);
    rc = init_pcm(&pcm_linein, &para);
    if(rc < 0){
        log_err("linein init pcm fail");
        return -1;
    }

    para.rate = SAMPLE_RATE;
    para.chn = LINEOUT_CHN;
    para.period_size = PERIOD_SIZE;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.format = SND_PCM_FORMAT_S16_LE;
    para.stream = SND_PCM_STREAM_PLAYBACK;
    para.mode = SND_PCM_NONBLOCK;
    sprintf(para.card_name, "plughw:%s,0", LINE_CARD_ID);
    rc = init_pcm(&pcm_lineout, &para);
    if(rc < 0){
        log_err("lineout init pcm fail");
        return -1;
    }

    para.rate = SAMPLE_RATE;
    para.chn = UAC_CHN;
    para.period_size = PERIOD_SIZE;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.format = SND_PCM_FORMAT_S16_LE;
    para.stream = SND_PCM_STREAM_CAPTURE;
    para.mode = SND_PCM_NONBLOCK;
    sprintf(para.card_name, "plughw:%s,0", UAC_CARD_ID);
    rc = init_pcm(&pcm_uacin, &para);
    if(rc < 0){
        log_err("uacin init pcm fail");
        return -1;
    }

    para.rate = SAMPLE_RATE;
    para.chn = UAC_CHN;
    para.period_size = PERIOD_SIZE;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.format = SND_PCM_FORMAT_S16_LE;
    para.stream = SND_PCM_STREAM_PLAYBACK;
    para.mode = SND_PCM_NONBLOCK;
    sprintf(para.card_name, "plughw:%s,0", UAC_CARD_ID);
    rc = init_pcm(&pcm_uacout, &para);
    if(rc < 0){
        log_err("uacout init pcm fail");
        return -1;
    }

    snd_pcm_start(pcm_linein);
    snd_pcm_start(pcm_uacin);

    while(state_task_other_in_and_out.running){
        avail_frames = snd_pcm_avail(pcm_linein);
        if(avail_frames >= PERIOD_SIZE){
            rc = pcm_in(pcm_linein, linein_buff, PERIOD_SIZE, "line-in");
            if(rc > 0){
                memset(tmpbuff, 0,sizeof(tmpbuff));
                for(i = 0; i < PERIOD_SIZE; i++){
                    tmpbuff[i] = linein_buff[i][0];
                }
                for(i = 0; i < algo_max; i++){
                    //left-chn ref
                    //right-chn linein
                    if(rb_get_space_free(g_ref_rngbuff[i]) < sizeof(tmpbuff)){
                        rb_discard(g_ref_rngbuff[i], sizeof(tmpbuff));
                    }
                    rb_write(g_ref_rngbuff[i], tmpbuff, sizeof(tmpbuff));
                }
            }
        }
        else if(avail_frames < 0){
            check_pcm_state(pcm_linein, avail_frames, "line-in");
        }

        avail_frames = snd_pcm_avail(pcm_uacin);
        if(avail_frames >= PERIOD_SIZE){
            rc = pcm_in(pcm_uacin, uacin_buff, PERIOD_SIZE, "uac-in");
            if(rc > 0){
                //
            }
        }
        else if(avail_frames < 0){
            check_pcm_state(pcm_uacin, avail_frames, "uac-in");
        }

        if(rb_get_space_used(g_algo_out_rngbuff) >= 2*PERIOD_SIZE){
            rb_read(g_algo_out_rngbuff, tmpbuff, sizeof(tmpbuff));
        }

        avail_frames = snd_pcm_avail(pcm_lineout);
        if(avail_frames >= PERIOD_SIZE){
            for(i = 0; i < PERIOD_SIZE; i++){
                lineout_buff[i][0] = tmpbuff[i];//left-chn aecout
                lineout_buff[i][1] = ((uacin_buff[i][0] + uacin_buff[i][1])/2) + linein_buff[i][1];
            }
            rc = pcm_out(pcm_lineout, lineout_buff, PERIOD_SIZE, "line-out");
            if(rc > 0){
                //
            }
        }
        else if(avail_frames < 0){
            check_pcm_state(pcm_lineout, avail_frames, "line-out");
        }

        avail_frames = snd_pcm_avail(pcm_uacout);
        if(avail_frames >= PERIOD_SIZE){
            for(i = 0; i < PERIOD_SIZE; i++){
                uacout_buff[i][0] = tmpbuff[i];//left-chn aecout
                uacout_buff[i][1] = tmpbuff[i];//left-chn aecout
            }
            rc = pcm_out(pcm_uacout, uacout_buff, PERIOD_SIZE, "uac-out");
            if(rc > 0){
                //
            }
        }
        else if(avail_frames < 0){
            check_pcm_state(pcm_uacout, avail_frames, "uac-out");
        }

        usleep(100);
    }

    destroy_pcm(pcm_linein);
    destroy_pcm(pcm_lineout);
    destroy_pcm(pcm_uacin);
    destroy_pcm(pcm_uacout);

    return 0;
}
static int init_task_other_in_and_out(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task_other_in_and_out, 0, sizeof(state_task_other_in_and_out));
    ret  = create_thread("task_other_in_and_out", cpu, high_priority, task_other_in_and_out, &state_task_other_in_and_out);
    if(ret != 0){
        log_warn("create %s thread failed\n", "task_other_in_and_out");
    }

    return ret;
}
static void exit_task_other_in_and_out(void)
{
    destroy_thread(&state_task_other_in_and_out);
}
/**************************************************************************/
static int task1(void *arg)
{
    int i, j, k;
    short mic_in_buff[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    short rec_buff[ALGO_PERIOD_SIZE][REC_CHN] = {0};
    short mic_data[7*ALGO_PERIOD_SIZE] = {0};
    short out_data[ALGO_PERIOD_SIZE] = {0};
    short ref_data[ALGO_PERIOD_SIZE] = {0};

    while(state_task1.running){
        if(rb_get_space_used(g_mic_rngbuff[algo_1]) < sizeof(mic_in_buff)){
            sem_wait(&g_sem_alg[algo_1]);
            continue;
        }
        memset(mic_in_buff, 0, sizeof(mic_in_buff));
        rb_read(g_mic_rngbuff[algo_1], mic_in_buff, sizeof(mic_in_buff));
        k = 0;
        for(i = 0; i < 7; i++){
            for(j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic_data[k] = mic_in_buff[j][i];
                rec_buff[j][i] = mic_in_buff[j][i];
            }
        }

        for(i = 0; i < ALGO_PERIOD_SIZE; i++){
            rec_buff[i][rec_ref] = ref_data[i];
        }
        
#if ENABLED_ALGO
        JD_MicArray_Process1(mic_data, out_data, ref_data);
#endif

        if(rb_get_space_free(g_algo_out_rngbuff) < sizeof(out_data)){
            rb_discard(g_algo_out_rngbuff, sizeof(out_data));
        }
        rb_write(g_algo_out_rngbuff, out_data, sizeof(out_data));

        for(i = 0; i < ALGO_PERIOD_SIZE; i++){
            rec_buff[i][rec_out] = out_data[i];
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            rb_write(g_rec_rngbuff, rec_buff, sizeof(rec_buff));
        }

        usleep(100);
    }

    return 0;
}
static int init_task1(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task1, 0, sizeof(state_task1));
    ret  = create_thread("task1", cpu, high_priority, task1, &state_task1);
    if(ret != 0){
        log_warn("create %s thread failed\n", "task1");
    }

    return ret;
}
static void exit_task1(void)
{
    destroy_thread(&state_task1);
}
/**************************************************************************/
static int task2(void *arg)
{
    int i,j,k;
    short mic_in_buff[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    short mic_data[7*ALGO_PERIOD_SIZE] = {0};
    short ref_data[ALGO_PERIOD_SIZE] = {0};

    while(state_task2.running){
        if(rb_get_space_used(g_mic_rngbuff[algo_2]) < sizeof(mic_in_buff)){
            sem_wait(&g_sem_alg[algo_2]);
            continue;
        }
        memset(mic_in_buff, 0, sizeof(mic_in_buff));
        rb_read(g_mic_rngbuff[algo_2], mic_in_buff, sizeof(mic_in_buff));
        k = 0;
        for(i = 0; i < 7; i++){
            for(j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic_data[k] = mic_in_buff[j][i];
            }
        }

#if ENABLED_ALGO
        JD_MicArray_Process2(mic_data, ref_data);
#endif

        usleep(100);
    }
    return 0;
}
static int init_task2(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task2, 0, sizeof(state_task2));
    ret  = create_thread("task2", cpu, high_priority, task2, &state_task2);
    if(ret != 0){
        log_warn("create %s thread failed\n", "task2");
    }

    return ret;
}
static void exit_task2(void)
{
    destroy_thread(&state_task2);
}
/************************************************************************************/
static int task3(void *arg)
{
    int i,j,k;
    short mic_in_buff[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    short mic_data[7*ALGO_PERIOD_SIZE] = {0};
    short ref_data[ALGO_PERIOD_SIZE] = {0};

    while(state_task3.running){
        if(rb_get_space_used(g_mic_rngbuff[algo_3]) < sizeof(mic_in_buff)){
            sem_wait(&g_sem_alg[algo_3]);
            continue;
        }
        memset(mic_in_buff, 0, sizeof(mic_in_buff));
        rb_read(g_mic_rngbuff[algo_3], mic_in_buff, sizeof(mic_in_buff));
        k = 0;
        for(i = 0; i < 7; i++){
            for(j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic_data[k] = mic_in_buff[j][i];
            }
        }
#if ENABLED_ALGO
        JD_MicArray_Process3(mic_data, ref_data);
#endif

        usleep(100);
    }
    return 0;
}
static int init_task3(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task3, 0, sizeof(state_task3));
    ret  = create_thread("task3", cpu, high_priority, task3, &state_task3);
    if(ret != 0){
        log_warn("create %s thread failed\n", "task3");
    }

    return ret;
}
static void exit_task3(void)
{
    destroy_thread(&state_task3);
}

static void rec_start(void)
{
	char path[256] = {0};

    snprintf(path, sizeof(path), "/data/rec.wav");
    if(!p_file_rec) {
        p_file_rec = fopen(path, "w");
        wav_start_write(p_file_rec, &st_wavhead, 16, REC_CHN, SAMPLE_RATE);
    }
}

static void rec_stop(void)
{
    if(p_file_rec){
        wav_stop_write(p_file_rec, &st_wavhead, total_size);
        fclose(p_file_rec);
        p_file_rec = NULL;
    }
}

static int task_rec(void *arg)
{
    prctl(PR_SET_NAME, "task_rec");
    log_info("task rec start\n");

    int wcnt,ret,poll_ms;
    short wrbuff[SAMPLE_RATE][REC_CHN];
    udp_server* recv_udp = NULL;
    char buf[1024] = {0};
    struct sockaddr_in client_addr;
    unsigned long cur_time, rest_time;
    static unsigned long start_record_time = 0;
    static unsigned long expect_time = 0;

    recv_udp = udp_server_init(UDP_SERVER_PORT);
    if(!recv_udp){
        log_err("record recv udp init error\n");
        exit(-1);
    }
    poll_ms = 1000; //select超时时间
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    while(state_task_rec.running){
        ret = udp_server_recv(recv_udp, &client_addr, buf, sizeof(buf), poll_ms);
        if(ret > 0){
            buf[sizeof(buf) - 1] = '\0';
            log_dbg("recv cmmd: %s\n", buf);
            if (strstr(buf, RECORD_CMD_START)) {
                start_record_time = get_sys_ms();
                cur_time = start_record_time;
                rec_start();
                rb_cleanup(g_rec_rngbuff);
                atomic_store(&g_record_action, algo_record_cmd_start);
                udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                log_dbg("send cmd: %s, ret:%d\n", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STOP)) {
                atomic_store(&g_record_action, algo_record_cmd_stop);
                ret = udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                log_dbg("send cmd: %s, ret:%d\n", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STATUS)) {
                if(atomic_load(&g_record_action) == algo_record_cmd_start)  {
                    cur_time = get_sys_ms();
                    rest_time = expect_time - (cur_time - start_record_time)/1000;
                    log_dbg("rest_time=%ld s\n", rest_time);
                    if(rest_time > 0){
                        snprintf(buf, sizeof(buf), "%s=%s %lds", RECORD_CMD_STATUS, "going", rest_time);
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        log_dbg("send cmd: %s, ret:%d\n", buf, ret);
                    }
                    else if(rest_time <= 0){
                        snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "end_of_record");
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        log_dbg("to end of recording\n");
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
                log_dbg("expect_time:%ld s\n", expect_time);
            }
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            if(rb_get_space_used(g_rec_rngbuff) >= sizeof(wrbuff)){
                rb_read(g_rec_rngbuff, wrbuff, sizeof(wrbuff));
                if(p_file_rec){
                    wcnt = fwrite(wrbuff, 1, sizeof(wrbuff), p_file_rec);
                    total_size += wcnt;
                    log_dbg("record data total_size=%ld\n", total_size);
                }
            }
        }
        else if(atomic_load(&g_record_action) == algo_record_cmd_stop){
            if(rb_get_space_used(g_rec_rngbuff) >= sizeof(wrbuff)){
                rb_read(g_rec_rngbuff, wrbuff, sizeof(wrbuff));
                if(p_file_rec){
                    wcnt = fwrite(wrbuff, 1, sizeof(wrbuff), p_file_rec);
                    total_size += wcnt;
                    log_dbg("record datain total_size=%ld\n", total_size);
                }
            }
            
            atomic_store(&g_record_action, algo_record_cmd_none);
            log_dbg("record complete, g_record_action=%d\n", atomic_load(&g_record_action));
            snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "finish");
            udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
            log_dbg("send cmd: %s, ret:%d\n", buf, ret);
            rec_stop();
        }
        usleep(100);
    }

    udp_server_exit(recv_udp);

    log_info("task rec exit\n");
    return 0;
}

int init_task_rec(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        log_warn("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&state_task_rec, 0, sizeof(state_task_rec));
    ret  = create_thread("task_rec", cpu, high_priority, task_rec, &state_task_rec);
    if(ret != 0){
        log_err("create %s thread failed\n", "task_rec");
    }

    return ret;
}

void exit_task_rec(void)
{
    destroy_thread(&state_task_rec);
}

void init_rngbuff(void)
{   
    for(int i = 0; i < algo_max; i++){
        g_mic_rngbuff[i] = rb_create(4*ALGO_PERIOD_SIZE*2*MIC_CHN);
        g_ref_rngbuff[i] = rb_create(4*ALGO_PERIOD_SIZE*2);
    }
    g_algo_out_rngbuff = rb_create(4*ALGO_PERIOD_SIZE*2);
    g_rec_rngbuff = rb_create(REC_RNGBUFF_SIZE*2*REC_CHN);
}

void destroy_rngbuff(void)
{
    for(int i = 0; i < algo_max; i++){
        rb_destroy(g_mic_rngbuff[i]);
        rb_destroy(g_ref_rngbuff[i]);
    }
    rb_destroy(g_algo_out_rngbuff);
    rb_destroy(g_rec_rngbuff);
}

static void write_file_str(const char *file, const char *str)
{
	FILE *fp;
	
	if(!file || !str) {
		return;
	}

	fp = fopen(file, "w");
	if(fp) {
		fwrite(str, strlen(str), 1, fp);
		fclose(fp);
	} else {
		log_err("write file=%s fail!!!\n", file);
	}
}

int main(int argc, char **argv)
{
    char info[256]={0};

    sprintf(info, "app_version: %s\nalgo_version: %s\nbuild_time: %s\n", APP_VERSION, JD_MicArray_GetVersion(), BUILD_TIME);
    log_info("%s", info);
    write_file_str("/tmp/version", info);
#if ENABLED_ALGO
    JD_MicArray_Init1();
    JD_MicArray_Init2();
    JD_MicArray_Init3();
#endif

    int i;
    for(i = 0; i < algo_max; i++){
        sem_init(&g_sem_alg[i], 0, 0);
    }
    init_rngbuff();
    init_led();
    init_task_rec(CPU0);
    init_task_mic(CPU0);
    init_task_other_in_and_out(CPU0);
    init_task1(CPU1);
    init_task2(CPU2);
    init_task3(CPU2);

    int led_sta = 1;
    while(1){
        sleep(1);
        led_sta = !led_sta;
        update_led_sta(led_sta);
        // printf("---- testing ----\n");
    }

    destroy_led();
    exit_task_rec();
    exit_task_other_in_and_out();
    exit_task1();
    exit_task2();
    exit_task3();
    exit_task_mic();
    destroy_rngbuff();
    for(i = 0; i < algo_max; i++){
        sem_destroy(&g_sem_alg[i]);
    }

#if ENABLED_ALGO
    JD_MicArray_Close1();
    JD_MicArray_Close2();
    JD_MicArray_Close3();
#endif

    printf("---- exit ----\n");

    return 0;
}