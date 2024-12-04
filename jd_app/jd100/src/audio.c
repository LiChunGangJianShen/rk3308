#include <semaphore.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include "log.h"
#include "thread.h"
#include "alsa_api.h"
#include "ringbuffer.h"
#include "comm.h"
#include "udp_server.h"
#include "wav_file.h"
#include "API.h"

#define SAMPLE_RATE 22050
#define PERIOD_SIZE 256
#define BUFFER_SIZE PERIOD_SIZE*4
#define ALG_FRAMES  256
#define ALG_COST_TIME   10

//Asound Card Name
#define CAPTURE_CARD_NAME  "default"   //default capture
#define PLAYBACK_CARD_NAME  "default"   //default playback
#define UAC_CARD_NAME  "uac"
//Channels
#define CAPTURE_CHN   10
#define PLAYBACK_CHN    2
#define UAC_CHN    2
#define ALGO_CHN    8//7mic+1ref

enum{
    alg_idx_1=0,
    alg_idx_2,
    alg_idx_3,
    alg_idx_max
};
static struct ringbuffer *rngbuff_ai[alg_idx_max] = {0};
#define AI_RNGBUFF_SIZE BUFFER_SIZE
static sem_t g_sem_3a[alg_idx_max];
static sem_t g_sem_alg_ready[alg_idx_max];
static sem_t g_sem_alg_ready_playback;

static struct ringbuffer *rngbuff_ao = 0;
static struct ringbuffer *rngbuff_ao_spk = 0;
#define AO_RNGBUFF_SIZE BUFFER_SIZE

//RECORD
#define REC_RNGBUFF_SIZE BUFFER_SIZE*1000
#define UDP_SERVER_PORT    (6000)
#define RECORD_CMD_START   "record-cmd-start"
#define RECORD_CMD_STOP    "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"
#define MAX_REC_TIME    90   //s

typedef enum {
    algo_record_cmd_none,
    algo_record_cmd_start,
    algo_record_cmd_stop,
}algo_record_cmd;
enum{
    rec_datain=0,
    rec_dataout
};
static struct ringbuffer *rngbuff_rec = 0;
static FILE *p_file_rec = NULL;
struct my_wave_file_headers st_wavhead;
static unsigned long total_size = 0;

struct my_wave_file_headers st_wavhead_dataout;
static unsigned long total_size_dataout = 0;
static struct ringbuffer *rngbuff_rec_dataout = 0;
static FILE *p_file_rec_dataout = NULL;

static atomic_int g_record_action = algo_record_cmd_none;

static pthread_state_t capture_task_state;
static pthread_state_t playback_task_state;
static pthread_state_t alg_task_state;
static pthread_state_t alg2_task_state;
static pthread_state_t alg3_task_state;
static pthread_state_t rec_task_state;


static void rec_start(void);
static void rec_stop(void);

static int capture_task(void *arg)
{
    prctl(PR_SET_NAME, "capture_task");
    logi("capture task start\n");

    int err,get_frames=0;
    snd_pcm_t *ppcm_capture = NULL;
    snd_pcm_t *ppcm_capture_uac = NULL;
    alsa_api_para_t params_capture;
    alsa_api_para_t params_capture_uac;
    audio_fmt_t algo_buff[PERIOD_SIZE][ALGO_CHN];
    audio_fmt_t capture_buff[PERIOD_SIZE][CAPTURE_CHN];
    audio_fmt_t capture_buff_uac[PERIOD_SIZE][UAC_CHN];
    audio_fmt_t lineinbuff[PERIOD_SIZE];
    audio_fmt_t uacbuff[PERIOD_SIZE];
    audio_fmt_t tmpbuff[PERIOD_SIZE];
    snd_pcm_sframes_t avail_frames = 0;

    memset(tmpbuff, 0, sizeof(tmpbuff));
    memset(lineinbuff, 0, sizeof(lineinbuff));
    memset(uacbuff, 0, sizeof(uacbuff));
    memset(algo_buff, 0, sizeof(algo_buff));
    memset(capture_buff, 0, sizeof(capture_buff));
    memset(capture_buff_uac, 0, sizeof(capture_buff_uac));

    for(int i = 0; i < alg_idx_max; i++){
        sem_wait(&g_sem_alg_ready[i]);
    }

    memset(&params_capture, 0, sizeof(params_capture));
    params_capture.block = SND_PCM_NONBLOCK;
    params_capture.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    params_capture.stream = SND_PCM_STREAM_CAPTURE;
    params_capture.format = SND_PCM_FORMAT_S16_LE;
    params_capture.chn = CAPTURE_CHN;
    params_capture.rate = SAMPLE_RATE;
    params_capture.period_size = PERIOD_SIZE;
    params_capture.buffer_size = BUFFER_SIZE;
    sprintf(params_capture.card_name, "%s", CAPTURE_CARD_NAME);
    if((err = init_pcm(&ppcm_capture, params_capture)) < 0){
        loge("init capture pcm error\n");
        goto err_to_exit;
    }

    memset(&params_capture_uac, 0, sizeof(params_capture_uac));
    params_capture_uac.block = SND_PCM_NONBLOCK;
    params_capture_uac.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    params_capture_uac.stream = SND_PCM_STREAM_CAPTURE;
    params_capture_uac.format = SND_PCM_FORMAT_S16_LE;
    params_capture_uac.chn = UAC_CHN;
    params_capture_uac.rate = SAMPLE_RATE;
    params_capture_uac.period_size = PERIOD_SIZE;
    params_capture_uac.buffer_size = BUFFER_SIZE;
    sprintf(params_capture_uac.card_name, "%s", UAC_CARD_NAME);
    if((err = init_pcm(&ppcm_capture_uac, params_capture_uac)) < 0){
        loge("init capture uac pcm error\n");
        goto err_to_exit;
    }

    snd_pcm_start(ppcm_capture);
    snd_pcm_start(ppcm_capture_uac);

    while (capture_task_state.running)
    {
        if(ppcm_capture){
            avail_frames = snd_pcm_avail(ppcm_capture);
            if(avail_frames >= PERIOD_SIZE){
                err = pcm_in(ppcm_capture, capture_buff, PERIOD_SIZE, CAPTURE_CARD_NAME);
                if(err > 0){
                    get_frames += err;
                    for(int i = 0; i < CAPTURE_CHN; i++){
                        for(int j = 0; j < PERIOD_SIZE; j++){
                            if(i <= 6){
                                algo_buff[j][i] = capture_buff[j][i];
                            }
                            else if(i == 8){
                                algo_buff[j][7] = capture_buff[j][i];
                            }
                            else if(i == 9){
                                lineinbuff[j] = capture_buff[j][i];
                            }
                        }
                    }
                    for(int i = 0; i < alg_idx_max; i++){
                        if(rb_get_space_free(rngbuff_ai[i]) < sizeof(algo_buff)){
                            rb_discard(rngbuff_ai[i], sizeof(algo_buff));
                        }
                        rb_write(rngbuff_ai[i], algo_buff, sizeof(algo_buff));
                    }
                }
            }
            else if(avail_frames < 0){
                logd("avail_frames=%ld,%s\n", avail_frames, snd_strerror(avail_frames));
                check_pcm_state(ppcm_capture, avail_frames, "capture");
            }
        }
        if(ppcm_capture_uac){
            avail_frames = snd_pcm_avail(ppcm_capture_uac);
            if(avail_frames >= PERIOD_SIZE){
                err = pcm_in(ppcm_capture_uac, capture_buff_uac, PERIOD_SIZE, UAC_CARD_NAME);
                if(err > 0){
                    for(int i = 0; i < PERIOD_SIZE; i++){
                        uacbuff[i] = (capture_buff_uac[i][0] + capture_buff_uac[i][1])/2;
                    }
                }
            }
            else if(avail_frames < 0){
                logd("avail_frames=%ld,%s\n", avail_frames, snd_strerror(avail_frames));
                check_pcm_state(ppcm_capture_uac, avail_frames, "capture uac");
            }
        }

        for(int i = i; i < PERIOD_SIZE; i++){
            tmpbuff[i] = lineinbuff[i] + uacbuff[i];
        }
        if(rb_get_space_free(rngbuff_ao_spk) < sizeof(tmpbuff)){
            rb_discard(rngbuff_ao_spk, sizeof(tmpbuff));
        }
        rb_write(rngbuff_ao_spk, tmpbuff, sizeof(tmpbuff));

        if(get_frames >= ALG_FRAMES){
            get_frames -= ALG_FRAMES;
            for(int i = 0; i < alg_idx_max; i++){
                sem_post(&g_sem_3a[i]);
            }
        }
        usleep(100);
    }
    
err_to_exit:
    if(ppcm_capture)
        snd_pcm_close(ppcm_capture);
    if(ppcm_capture_uac)
        snd_pcm_close(ppcm_capture_uac);
    logi("capture task stop\n");

    return 0;
}

int capture_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&capture_task_state, 0, sizeof(capture_task_state));
    ret  = create_thread("capture_task", cpu, high_priority, capture_task, &capture_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "capture_task");
    }

    return ret;
}

void capture_task_exit(void)
{
    destroy_thread(&capture_task_state);
}

static int playback_task(void *arg)
{
    prctl(PR_SET_NAME, "playback_task");
    logi("playback task start\n");

    int err;
    snd_pcm_t *ppcm_playback;
    snd_pcm_t *ppcm_playback_uac;
    alsa_api_para_t params_playback;
    alsa_api_para_t params_playback_uac;
    audio_fmt_t playback_buff[PERIOD_SIZE][PLAYBACK_CHN];
    audio_fmt_t playback_buff_uac[PERIOD_SIZE][UAC_CHN];
    audio_fmt_t tmpbuff[2][PERIOD_SIZE];
    snd_pcm_sframes_t avail_frames;

    // sem_wait(&g_sem_alg_ready_playback);

    memset(playback_buff, 0, sizeof(playback_buff));
    memset(tmpbuff, 0, sizeof(tmpbuff));
    memset(&params_playback, 0, sizeof(params_playback));
    params_playback.block = SND_PCM_NONBLOCK;
    params_playback.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    params_playback.stream = SND_PCM_STREAM_PLAYBACK;
    params_playback.format = SND_PCM_FORMAT_S16_LE;
    params_playback.chn = PLAYBACK_CHN;
    params_playback.rate = SAMPLE_RATE;
    params_playback.period_size = PERIOD_SIZE;
    params_playback.buffer_size = BUFFER_SIZE;
    sprintf(params_playback.card_name, "%s", PLAYBACK_CARD_NAME);
    if((err = init_pcm(&ppcm_playback, params_playback)) < 0){
        loge("init playback pcm error\n");
        goto err_to_exit;
    }

    memset(playback_buff_uac, 0, sizeof(playback_buff_uac));
    memset(&params_playback_uac, 0, sizeof(params_playback_uac));
    params_playback_uac.block = SND_PCM_NONBLOCK;
    params_playback_uac.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    params_playback_uac.stream = SND_PCM_STREAM_PLAYBACK;
    params_playback_uac.format = SND_PCM_FORMAT_S16_LE;
    params_playback_uac.chn = UAC_CHN;
    params_playback_uac.rate = SAMPLE_RATE;
    params_playback_uac.period_size = PERIOD_SIZE;
    params_playback_uac.buffer_size = BUFFER_SIZE;
    sprintf(params_playback_uac.card_name, "%s", UAC_CARD_NAME);
    if((err = init_pcm(&ppcm_playback_uac, params_playback_uac)) < 0){
        loge("init playback uac pcm error\n");
        goto err_to_exit;
    }

    while (playback_task_state.running)
    {
        if(rb_get_space_used(rngbuff_ao) >= sizeof(tmpbuff[0])){
            rb_read(rngbuff_ao, tmpbuff[0], sizeof(tmpbuff[0]));
        }
        else{
            memset(tmpbuff[0], 0, sizeof(tmpbuff[0]));
        }

        if(rb_get_space_used(rngbuff_ao_spk) >= sizeof(tmpbuff[1])){
            rb_read(rngbuff_ao_spk, tmpbuff[1], sizeof(tmpbuff[1]));
        }
        else{
            memset(tmpbuff[1], 0, sizeof(tmpbuff[1]));
        }

        for(int i = 0; i < PLAYBACK_CHN; i++){
            for(int j = 0; j < PERIOD_SIZE; j++){
                playback_buff[j][i] = tmpbuff[i][j];
                playback_buff_uac[j][i] = tmpbuff[1][j];
            }
        }

        if(ppcm_playback){
            avail_frames = snd_pcm_avail(ppcm_playback);
            if(avail_frames >= PERIOD_SIZE){
                pcm_out(ppcm_playback, playback_buff, PERIOD_SIZE, PLAYBACK_CARD_NAME);
            }
        }
        if(ppcm_playback_uac){
            avail_frames = snd_pcm_avail(ppcm_playback_uac);
            if(avail_frames >= PERIOD_SIZE){
                pcm_out(ppcm_playback_uac, playback_buff_uac, PERIOD_SIZE, UAC_CARD_NAME);
            }
        }
        usleep(100);
    }
    
err_to_exit:
    if(ppcm_playback)
        snd_pcm_close(ppcm_playback);
    if(ppcm_playback_uac)
        snd_pcm_close(ppcm_playback_uac);
    logi("playback task stop\n");
    return 0;
}

int playback_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&playback_task_state, 0, sizeof(playback_task_state));
    ret  = create_thread("playback_task", cpu, high_priority, playback_task, &playback_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "playback_task");
    }

    return ret;
}

void playback_task_exit(void)
{
    sem_destroy(&g_sem_alg_ready_playback);
    destroy_thread(&playback_task_state);
}

static int alg_task(void *arg)
{
    prctl(PR_SET_NAME, "alg_task");
    logi("alg task start\n");

    int i,j,k;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb;
    audio_fmt_t algo_buff[ALG_FRAMES][ALGO_CHN];
    audio_fmt_t dataout_buff[ALG_FRAMES];
    audio_fmt_t mic_buff[7*ALG_FRAMES];
    audio_fmt_t ref_buff[ALG_FRAMES];

    memset(mic_buff, 0, sizeof(mic_buff));
    memset(ref_buff, 0, sizeof(ref_buff));
    memset(algo_buff, 0, sizeof(algo_buff));
    memset(dataout_buff, 0, sizeof(dataout_buff));

    sem_post(&g_sem_alg_ready[alg_idx_1]);
    sem_post(&g_sem_alg_ready_playback);

    while(alg_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_1]) < sizeof(algo_buff)){
            sem_wait(&g_sem_3a[alg_idx_1]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_1]) >= sizeof(algo_buff)){
            memset(algo_buff, 0, sizeof(algo_buff));
            rb_read(rngbuff_ai[alg_idx_1], algo_buff, sizeof(algo_buff));
            k = 0;
            memset(mic_buff, 0, sizeof(mic_buff));
            memset(ref_buff, 0, sizeof(ref_buff));
            for(i = 0; i < ALGO_CHN; i++){
                if(i < 7){
                    for(j = 0; j < ALG_FRAMES; j++){
                        mic_buff[k] = algo_buff[j][i];
                        k++;
                    }
                }
                else{
                    for(j = 0; j < ALG_FRAMES; j++){
                        ref_buff[j] = algo_buff[j][i];
                    }
                }
            }

            gettimeofday(&tva, NULL);
#if ENABLE_ALGO
            JD_MicArray_Process1(mic_buff, ref_buff, dataout_buff);
#endif
            gettimeofday(&tvb, NULL);

            cost_time = check_time_increment_ms(tva, tvb);
            if(cost_time > ALG_COST_TIME){
                timeout_cnt++;
                logd("alg timeout(%ld), timeout_cnt(%ld)\n", cost_time, timeout_cnt);
            }
            // else{
            //     logd("alg cost time(%ld), k=%d\n", cost_time, k);
            // }

            if(rb_get_space_free(rngbuff_ao) < sizeof(dataout_buff)){
                rb_discard(rngbuff_ao, sizeof(dataout_buff));
            }
            rb_write(rngbuff_ao, dataout_buff, sizeof(dataout_buff));

            if(atomic_load(&g_record_action) == algo_record_cmd_start){
                rb_write(rngbuff_rec, algo_buff, sizeof(algo_buff));
                rb_write(rngbuff_rec_dataout, dataout_buff, sizeof(dataout_buff));
            }
        }

        usleep(100);
    }

    logi("alg task exit\n");
    return 0;
}

int alg_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg_task_state, 0, sizeof(alg_task_state));
    ret  = create_thread("alg_task", cpu, high_priority, alg_task, &alg_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "alg_task");
    }

    return ret;
}

void alg_task_exit(void)
{
    destroy_thread(&alg_task_state);
}

static int alg2_task(void *arg)
{
    prctl(PR_SET_NAME, "alg2_task");
    logi("alg2 task start\n");

    int i,j,k;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb;   
    audio_fmt_t ref_buff[ALG_FRAMES];
    audio_fmt_t algo_buff[ALG_FRAMES][ALGO_CHN];
    audio_fmt_t mic_buff[ALG_FRAMES*7];

    memset(ref_buff, 0, sizeof(ref_buff));
    memset(mic_buff, 0, sizeof(mic_buff));
    memset(algo_buff, 0, sizeof(algo_buff));

    sem_post(&g_sem_alg_ready[alg_idx_2]); 

    while(alg2_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_2]) < sizeof(algo_buff)){
            sem_wait(&g_sem_3a[alg_idx_2]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_2]) >= sizeof(algo_buff)){
            memset(algo_buff, 0, sizeof(algo_buff));
            rb_read(rngbuff_ai[alg_idx_2], algo_buff, sizeof(algo_buff));
            k = 0;
            memset(mic_buff, 0, sizeof(mic_buff));
            memset(ref_buff, 0, sizeof(ref_buff));
            for(i = 0; i < ALGO_CHN; i++){
                if(i < 7){
                    for(j = 0; j < ALG_FRAMES; j++){
                        mic_buff[k] = algo_buff[j][i];
                        k++;
                    }
                }
                else{
                    for(j = 0; j < ALG_FRAMES; j++){
                        ref_buff[j] += algo_buff[j][i];
                    }
                }
            }
            gettimeofday(&tva, NULL);
#if ENABLE_ALGO
            JD_MicArray_Process2(mic_buff, ref_buff);
#endif
            gettimeofday(&tvb, NULL);

            cost_time = check_time_increment_ms(tva, tvb);
            if(cost_time > ALG_COST_TIME){
                timeout_cnt++;
                logd("alg2 timeout(%ld), timeout_cnt(%ld)\n", cost_time, timeout_cnt);
            }
            // else{
            //     logd("alg2 cost time(%ld), k=%d\n", cost_time, k);
            // }
        }

        usleep(100);
    }

    logi("alg2 task exit\n");
    return 0;
}

int alg2_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg2_task_state, 0, sizeof(alg2_task_state));
    ret  = create_thread("alg2_task", cpu, high_priority, alg2_task, &alg2_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "alg2_task");
    }

    return ret;
}

void alg2_task_exit(void)
{
    destroy_thread(&alg2_task_state);
}

static int alg3_task(void *arg)
{
    prctl(PR_SET_NAME, "alg3_task");
    logi("alg3 task start\n");

    int i,j,k;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb;   
    audio_fmt_t ref_buff[ALG_FRAMES];
    audio_fmt_t algo_buff[ALG_FRAMES][ALGO_CHN];
    audio_fmt_t mic_buff[ALG_FRAMES*10];

    memset(ref_buff, 0, sizeof(ref_buff));
    memset(mic_buff, 0, sizeof(mic_buff));
    memset(algo_buff, 0, sizeof(algo_buff));

    sem_post(&g_sem_alg_ready[alg_idx_3]); 

    while(alg2_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_3]) < sizeof(algo_buff)){
            sem_wait(&g_sem_3a[alg_idx_3]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_3]) >= sizeof(algo_buff)){
            memset(algo_buff, 0, sizeof(algo_buff));
            rb_read(rngbuff_ai[alg_idx_3], algo_buff, sizeof(algo_buff));
            k = 0;
            memset(mic_buff, 0, sizeof(mic_buff));
            memset(ref_buff, 0, sizeof(ref_buff));
            for(i = 0; i < ALGO_CHN; i++){
                if(i < 7){
                    for(j = 0; j < ALG_FRAMES; j++){
                        mic_buff[k] = algo_buff[j][i];
                        k++;
                    }
                }
                else{
                    for(j = 0; j < ALG_FRAMES; j++){
                        ref_buff[j] = algo_buff[j][i];
                    }
                }
            }

            gettimeofday(&tva, NULL);
#if ENABLE_ALGO
            JD_MicArray_Process3(mic_buff, ref_buff);
#endif
            gettimeofday(&tvb, NULL);

            cost_time = check_time_increment_ms(tva, tvb);
            if(cost_time > ALG_COST_TIME){
                timeout_cnt++;
                logd("alg3 timeout(%ld), timeout_cnt(%ld)\n", cost_time, timeout_cnt);
            }
            // else{
            //     logd("alg3 cost time(%ld), k=%d\n", cost_time, k);
            // }
        }

        usleep(100);
    }

    logi("alg3 task exit\n");
    return 0;
}

int alg3_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg3_task_state, 0, sizeof(alg3_task_state));
    ret  = create_thread("alg3_task", cpu, high_priority, alg3_task, &alg3_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "alg3_task");
    }

    return ret;
}

void alg3_task_exit(void)
{
    destroy_thread(&alg3_task_state);
}

static void rec_start(void)
{
	char path[256] = {0};

    snprintf(path, sizeof(path), "/data/algo_in.wav");
    if(!p_file_rec) {
        p_file_rec = fopen(path, "w");
        wav_start_write(p_file_rec, &st_wavhead, 16, ALGO_CHN, SAMPLE_RATE);
    }

    snprintf(path, sizeof(path), "/data/algo_out.wav");
    if(!p_file_rec_dataout) {
        p_file_rec_dataout = fopen(path, "w");
        wav_start_write(p_file_rec_dataout, &st_wavhead_dataout, 16, 1, SAMPLE_RATE);
    }
}

static void rec_stop(void)
{
    if(p_file_rec){
        wav_stop_write(p_file_rec, &st_wavhead, total_size);
        fclose(p_file_rec);
        p_file_rec = NULL;
    }
    if(p_file_rec_dataout){
        wav_stop_write(p_file_rec_dataout, &st_wavhead_dataout, total_size_dataout);
        fclose(p_file_rec_dataout);
        p_file_rec_dataout = NULL;
    }
}

static int rec_task(void *arg)
{
    prctl(PR_SET_NAME, "rec_task");
    logi("rec task start\n");

    int wcnt,ret,poll_ms;
    audio_fmt_t wrbuff[SAMPLE_RATE][ALGO_CHN];
    audio_fmt_t wrdataoutbuff[SAMPLE_RATE];
    udp_server* recv_udp = NULL;
    char buf[1024] = {0};
    struct sockaddr_in client_addr;
    unsigned long cur_time, rest_time;
    static unsigned long start_record_time = 0;
    static unsigned long expect_time = 0;

    recv_udp = udp_server_init(UDP_SERVER_PORT);
    if(!recv_udp){
        loge("record recv udp init error\n");
        exit(-1);
    }
    poll_ms = 1000; //select超时时间
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    while(rec_task_state.running){
        ret = udp_server_recv(recv_udp, &client_addr, buf, sizeof(buf), poll_ms);
        if(ret > 0){
            buf[sizeof(buf) - 1] = '\0';
            logd("recv cmmd: %s\n", buf);
            if (strstr(buf, RECORD_CMD_START)) {
                start_record_time = get_sys_ms();
                cur_time = start_record_time;
                rec_start();
                rb_cleanup(rngbuff_rec);
                rb_cleanup(rngbuff_rec_dataout);
                atomic_store(&g_record_action, algo_record_cmd_start);
                udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                logd("send cmd: %s, ret:%d\n", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STOP)) {
                atomic_store(&g_record_action, algo_record_cmd_stop);
                ret = udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                logd("send cmd: %s, ret:%d\n", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STATUS)) {
                if(atomic_load(&g_record_action) == algo_record_cmd_start)  {
                    cur_time = get_sys_ms();
                    rest_time = expect_time - (cur_time - start_record_time)/1000;
                    logd("rest_time=%ld s\n", rest_time);
                    if(rest_time > 0){
                        snprintf(buf, sizeof(buf), "%s=%s %lds", RECORD_CMD_STATUS, "going", rest_time);
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        logd("send cmd: %s, ret:%d\n", buf, ret);
                    }
                    else if(rest_time <= 0){
                        snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "end_of_record");
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        logd("to end of recording\n");
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
                logd("expect_time:%ld s\n", expect_time);
            }
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            if(rb_get_space_used(rngbuff_rec) >= sizeof(wrbuff)){
                rb_read(rngbuff_rec, wrbuff, sizeof(wrbuff));
                if(p_file_rec){
                    wcnt = fwrite(wrbuff, 1, sizeof(wrbuff), p_file_rec);
                    total_size += wcnt;
                    logd("record data total_size=%ld\n", total_size);
                }
            }
            if(rb_get_space_used(rngbuff_rec_dataout) >= sizeof(wrdataoutbuff)){
                rb_read(rngbuff_rec_dataout, wrdataoutbuff, sizeof(wrdataoutbuff));
                if(p_file_rec_dataout){
                    wcnt = fwrite(wrdataoutbuff, 1, sizeof(wrdataoutbuff), p_file_rec_dataout);
                    total_size_dataout += wcnt;
                    logd("record dataout total_size=%ld\n", total_size_dataout);
                }
            }
        }
        else if(atomic_load(&g_record_action) == algo_record_cmd_stop){
            if(rb_get_space_used(rngbuff_rec) >= sizeof(wrbuff)){
                rb_read(rngbuff_rec, wrbuff, sizeof(wrbuff));
                if(p_file_rec){
                    wcnt = fwrite(wrbuff, 1, sizeof(wrbuff), p_file_rec);
                    total_size += wcnt;
                    logd("record datain total_size=%ld\n", total_size);
                }
            }
            if(rb_get_space_used(rngbuff_rec_dataout) >= sizeof(wrdataoutbuff)){
                rb_read(rngbuff_rec_dataout, wrdataoutbuff, sizeof(wrdataoutbuff));
                if(p_file_rec_dataout){
                    wcnt = fwrite(wrdataoutbuff, 1, sizeof(wrdataoutbuff), p_file_rec_dataout);
                    total_size_dataout += wcnt;
                    logd("record dataout total_size=%ld\n", total_size_dataout);
                }
            }
            
            atomic_store(&g_record_action, algo_record_cmd_none);
            logd("record complete, g_record_action=%d\n", atomic_load(&g_record_action));
            snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "finish");
            udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
            logd("send cmd: %s, ret:%d\n", buf, ret);
            rec_stop();
        }
        usleep(100);
    }

    udp_server_exit(recv_udp);

    logi("rec task exit\n");
    return 0;
}

int rec_task_init(int cpu)
{
    int ret = 0;
    int high_priority = TRUE;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&rec_task_state, 0, sizeof(rec_task_state));
    ret  = create_thread("rec_task", cpu, high_priority, rec_task, &rec_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "rec_task");
    }

    return ret;
}

void rec_task_exit(void)
{
    destroy_thread(&rec_task_state);
}

void _sem_init(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        sem_init(&g_sem_3a[i], 0, 0);
        sem_init(&g_sem_alg_ready[i], 0, 0);
    }
    sem_init(&g_sem_alg_ready_playback, 0, 0);
}

void _sem_destroy(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        sem_destroy(&g_sem_3a[i]);
        sem_destroy(&g_sem_alg_ready[i]);
    }
    sem_destroy(&g_sem_alg_ready_playback);
}

void init_rngbuff(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        rngbuff_ai[i] = rb_create(sizeof(audio_fmt_t)*AI_RNGBUFF_SIZE*ALGO_CHN);
    }
    rngbuff_rec = rb_create(sizeof(audio_fmt_t)*REC_RNGBUFF_SIZE*ALGO_CHN);
    rngbuff_rec_dataout = rb_create(sizeof(audio_fmt_t)*REC_RNGBUFF_SIZE);
    rngbuff_ao = rb_create(sizeof(audio_fmt_t)*AO_RNGBUFF_SIZE);
    rngbuff_ao_spk = rb_create(sizeof(audio_fmt_t)*AO_RNGBUFF_SIZE);
}

void exit_rngbuff(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        rb_destroy(rngbuff_ai[i]);
    }
    rb_destroy(rngbuff_rec);
    rb_destroy(rngbuff_rec_dataout);
    rb_destroy(rngbuff_ao);
    rb_destroy(rngbuff_ao_spk);
}