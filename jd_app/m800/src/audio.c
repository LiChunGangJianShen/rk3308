#include <alsa/asoundlib.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <math.h>
#include "log.h"
#include "audio.h"
#include "ringbuf.h"
#include "thread.h"
#include "comm.h"
#include "algo.h"
#include "uevent.h"
#include "udp_server.h"
#include "wav_file.h"
#include "es8327.h"
#include "es7210.h"
#include "ti3104.h"
#include "out_ctrl.h"
#include "eq.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI    3.1415926
#endif
//RECORD
#define UDP_SERVER_PORT    6000
#define RECORD_CMD_START   "record-cmd-start"
#define RECORD_CMD_STOP    "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"
#define MAX_REC_TIME        60
#define REC_CHN (MIC_CNT+REF_CNT+OUT_CNT)

typedef enum {
    algo_record_cmd_none,
    algo_record_cmd_start,
    algo_record_cmd_stop,
}algo_record_cmd;
typedef enum
{
    rec_mic1=0,
    rec_mic2,
    rec_mic3,
    rec_mic4,
    rec_mic5,
    rec_mic6,
    rec_mic7,
#if MIC_CNT==8
    rec_mic8,
#endif
    rec_ref,
    rec_out,
    rec_max,
}rec_idx_t;

typedef short fmt_t;
typedef enum
{
    algo_1=0,
    algo_2,
    algo_3,
    algo_max,
}algo_idx_t;

static const float ALGO_TIME_MS = (float)(ALGO_PERIOD_SIZE / (SAMPLE_RATE / 1000.0f));
static const float SAMPLE_PER_MS_FLOAT = SAMPLE_RATE / 1000.0f;
static const char *g_mic_card_id = "rockchipes7210";
#if EN_UAC
static const char *g_uac_card_id = "UAC1Gadget";
#endif
static const char *g_line_card_id = "rockchipti3104";

static ringbuf_t *g_ringbuf_mic[algo_max] = {NULL};
static ringbuf_t *g_ringbuf_line_ref[algo_max] = {NULL};
#if EN_UAC
static ringbuf_t *g_ringbuf_uac_ao = NULL;
#endif
static ringbuf_t *g_ringbuf_line_ao = NULL;
static ringbuf_t *g_ringbuf_rec = NULL;
static ringbuf_t *g_ringbuf_fill_data = NULL;
static pthread_state_t g_algo1_task_state;
static pthread_state_t g_algo2_task_state;
static pthread_state_t g_algo3_task_state;
static pthread_state_t g_mic_task_state;
static pthread_state_t g_line_ai_task_state;
#if EN_UAC
static pthread_state_t g_uac_ao_task_state;
#endif
static pthread_state_t g_line_ao_task_state;
static pthread_state_t g_rec_task_state;
static sem_t g_sem_algo[algo_max];
static FILE *p_file_rec = NULL;
struct my_wave_file_headers st_wavhead;
static unsigned long total_size = 0;
static atomic_int g_record_action = algo_record_cmd_none;
static int g_line_capture_start = 0;

static void task_destroy(pthread_state_t *state);

static int pcm_params_setup(
    unsigned int *rate,
    unsigned int chn,
    snd_pcm_stream_t stream,
    snd_pcm_access_t access,
    snd_pcm_format_t format,
    snd_pcm_uframes_t *period_size,
    snd_pcm_uframes_t *buffer_size,
    snd_pcm_t *pcm,
    const char *aliases
)
{
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    int ret = 0;

    snd_pcm_hw_params_alloca(&hw_params);
    ret = snd_pcm_hw_params_any(pcm, hw_params);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_any error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    ret = snd_pcm_hw_params_set_access(pcm, hw_params, access);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_access error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    ret = snd_pcm_hw_params_set_format(pcm, hw_params, format);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_format error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    int rrate = *rate;
    int dir = 0;
    ret = snd_pcm_hw_params_set_rate_near(pcm, hw_params, rate, &dir);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_rate_near error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }
    if(rrate != *rate){
        loge("(%s:%s)Can't set rate(%d), really set rate(%d)\n", 
            aliases, snd_pcm_stream_name(stream), rrate, *rate);
    }

    ret = snd_pcm_hw_params_set_channels(pcm, hw_params, chn);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_channels error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    snd_pcm_uframes_t size = *period_size;
    ret = snd_pcm_hw_params_set_period_size_near(pcm, hw_params, period_size, &dir);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_period_size_near error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }
    if(size != *period_size){
        loge("(%s:%s)Can't set period-size(%d), really set period-size(%d)\n", 
            aliases, snd_pcm_stream_name(stream), size, *period_size);
    }

    size = *buffer_size;
    ret = snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, buffer_size);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params_set_buffer_size_near error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }
    if(size != *buffer_size){
        loge("(%s:%s)Can't set buffer-size(%d), really set buffer-size(%d)\n", 
            aliases, snd_pcm_stream_name(stream), size, *buffer_size);
    }

    ret = snd_pcm_hw_params(pcm, hw_params);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_hw_params error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    snd_pcm_sw_params_alloca(&sw_params);
    ret = snd_pcm_sw_params_current(pcm, sw_params);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_sw_params_current error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

    int val = 0;
    if(stream == SND_PCM_STREAM_CAPTURE){
        val = 1;
        ret = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, val);
        if(ret < 0){
            loge("(%s:%s)snd_pcm_sw_params_set_start_threshold(%d) error(%s)\n", 
                aliases, snd_pcm_stream_name(stream), val, snd_strerror(ret));
            goto __error;
        }

        val = *buffer_size;
        ret = snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, val);
        if(ret < 0){
            loge("(%s:%s)snd_pcm_sw_params_set_stop_threshold(%d) error(%s)\n", 
                aliases, snd_pcm_stream_name(stream), val, snd_strerror(ret));
            goto __error;
        }
    }
    else{
        val = *period_size+1;
        ret = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, val);
        if(ret < 0){
            loge("(%s:%s)snd_pcm_sw_params_set_start_threshold(%d) error(%s)\n", 
                aliases, snd_pcm_stream_name(stream), val, snd_strerror(ret));
            goto __error;
        }

        val = *buffer_size;
        ret = snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, val);
        if(ret < 0){
            loge("(%s:%s)snd_pcm_sw_params_set_stop_threshold(%d) error(%s)\n", 
                aliases, snd_pcm_stream_name(stream), val, snd_strerror(ret));
            goto __error;
        }
    }

    ret = snd_pcm_sw_params(pcm, sw_params);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_sw_params error(%s)\n", 
            aliases, snd_pcm_stream_name(stream), snd_strerror(ret));
        goto __error;
    }

__error:
    return ret;
}

static int mic_pcm_init(snd_pcm_t **pcm)
{
    int ret = 0;
    char name[32] = {0};
    int open_mode = 0;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    snd_pcm_access_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t period_size = MIC_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = PERIODS * period_size;
    unsigned int rate = SAMPLE_RATE;

    snprintf(name, sizeof(name), "hw:%d,0", snd_card_get_index(g_mic_card_id));
    name[sizeof(name) - 1] = '\0';
    open_mode = SND_PCM_NONBLOCK;
    ret = snd_pcm_open(pcm, name, stream, open_mode);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_open error(%s)\n", 
        g_mic_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    ret = pcm_params_setup(&rate, 
                        MIC_CHN,
                        stream,
                        access,
                        format,
                        &period_size,
                        &buffer_size,
                        *pcm,
                        g_mic_card_id);

    if(ret < 0){
        loge("(%s:%s)pcm_params_setup error(%s)\n", 
        g_mic_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    logi("mic pcm init success\n");
    return 0;
}

static int line_ref_pcm_init(snd_pcm_t **pcm)
{
    int ret = 0;
    char name[32] = {0};
    int open_mode = 0;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    snd_pcm_access_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t period_size = LINE_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = PERIODS * period_size;
    unsigned int rate = SAMPLE_RATE;

    snprintf(name, sizeof(name), "hw:%d,0", snd_card_get_index(g_line_card_id));
    name[sizeof(name) - 1] = '\0';
    open_mode = SND_PCM_NONBLOCK;
    ret = snd_pcm_open(pcm, name, stream, open_mode);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_open error(%s)\n", 
        g_line_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    ret = pcm_params_setup(&rate, 
                        LINE_AI_CHN,
                        stream,
                        access,
                        format,
                        &period_size,
                        &buffer_size,
                        *pcm,
                        g_line_card_id);

    if(ret < 0){
        loge("(%s:%s)pcm_params_setup error(%s)\n", 
        g_line_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    logi("line-ref pcm init success\n");
    return 0;
}

#if EN_UAC
static int uac_ao_pcm_init(snd_pcm_t **pcm)
{
    int ret = 0;
    char name[32] = {0};
    int open_mode = 0;
    snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
    snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    snd_pcm_access_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t period_size = UAC_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = PERIODS * period_size;
    unsigned int rate = SAMPLE_RATE;

    snprintf(name, sizeof(name), "hw:%d,0", snd_card_get_index(g_uac_card_id));
    name[sizeof(name) - 1] = '\0';
    open_mode = SND_PCM_NONBLOCK;
    ret = snd_pcm_open(pcm, name, stream, open_mode);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_open error(%s)\n", 
        g_uac_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    ret = pcm_params_setup(&rate, 
                        UAC_AO_CHN,
                        stream,
                        access,
                        format,
                        &period_size,
                        &buffer_size,
                        *pcm,
                        g_uac_card_id);

    if(ret < 0){
        loge("(%s:%s)pcm_params_setup error(%s)\n", 
        g_uac_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    logi("uac ao pcm init success\n");
    return 0;
}
#endif

static int line_ao_pcm_init(snd_pcm_t **pcm)
{
    int ret = 0;
    char name[32] = {0};
    int open_mode = 0;
    snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
    snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    snd_pcm_access_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t period_size = LINE_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = PERIODS * period_size;
    unsigned int rate = SAMPLE_RATE;

    snprintf(name, sizeof(name), "hw:%d,0", snd_card_get_index(g_line_card_id));
    name[sizeof(name) - 1] = '\0';
    open_mode = SND_PCM_NONBLOCK;
    ret = snd_pcm_open(pcm, name, stream, open_mode);
    if(ret < 0){
        loge("(%s:%s)snd_pcm_open error(%s)\n", 
        g_line_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    ret = pcm_params_setup(&rate, 
                        LINE_AO_CHN,
                        stream,
                        access,
                        format,
                        &period_size,
                        &buffer_size,
                        *pcm,
                        g_line_card_id);

    if(ret < 0){
        loge("(%s:%s)pcm_params_setup error(%s)\n", 
        g_line_card_id, snd_pcm_stream_name(stream), snd_strerror(ret));
        return -1;
    }

    logi("line ao pcm init success\n");
    return 0;
}

static void pcm_start(snd_pcm_t *pcm, const char *aliases)
{
    if(pcm == NULL){
        return;
    }

    {
        snd_pcm_state_t state = 0;
        int start_err = 0;
        int retry = 0;

        start_err = snd_pcm_start(pcm);
        if(start_err){
            loge("%s first time start pcm error\n", aliases);
        }
        while(retry < 20){
            state = snd_pcm_state(pcm);
            if(state == SND_PCM_STATE_RUNNING){
                logi("%s pcm(capture stream) state: %s\n", aliases, snd_pcm_state_name(state));
                break;
            }
            if(start_err < 0){
                start_err = snd_pcm_start(pcm);
                if(start_err >= 0){
                    logi("%s restry snd_pcm_start success\n", aliases);
                }
            }

            retry++;
            delay_ms(100);
        }
    }
}

static int check_and_recover_pcm(snd_pcm_t *pcm, int err, const char *aliases)
{
    if(!pcm){
        loge("pcm null\n");
        return -1;
    }

    switch (err)
    {
    case -EPIPE:
        err = snd_pcm_prepare(pcm);
        if(err < 0){
            logw("%s Can't recovery from xrun state(%s)\n", aliases, snd_strerror(err));
            return -1;
        }
        delay_ms(5);
        snd_pcm_start(pcm);
        break;
    case -EIO:
    case -EBADFD:
        logw("%s I/O Error(%s)\n", aliases, snd_strerror(err));
        return -1;
    case -EINVAL:
        logw("%s Invalid Para Error(%s)\n", aliases, snd_strerror(err));
        return -1;
    case -ESTRPIPE:
        err = snd_pcm_recover(pcm, err, 0);
        if(err < 0){
            logw("%s recover from suspend failed(%s)\n", aliases, snd_strerror(err));
            return -1;
        }
        break;
    default:
        logw("%s other error(%s)\n", aliases, snd_strerror(err));
        err = snd_pcm_recover(pcm, err, 0);
        if(err < 0){
            logw("%s recover from other error failed(%s)\n", aliases, snd_strerror(err));
            return -1;
        }
        break;
    }

    return 0;
}

static void safe_cap_pcm_close(snd_pcm_t **pcm)
{
    if(!pcm || !*pcm){
        return;
    }
    snd_pcm_drop(*pcm);
    snd_pcm_close(*pcm);
    *pcm = NULL;
}

static void safe_play_pcm_close(snd_pcm_t **pcm)
{
    if(!pcm || !*pcm){
        return;
    }
    snd_pcm_drain(*pcm);
    snd_pcm_close(*pcm);
    *pcm = NULL;
}

static ringbuf_t *ringbuf_init(int size, const char *aliases)
{
    if(size == 0){
        return NULL;
    }

    ringbuf_t *buf = ringbuf_create(size);
    if(buf == NULL){
        loge("(%s) ringbuf create error\n", aliases);
        return NULL;
    }

    return buf;
}

static int mic_task(void *arg)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_t *pcm_play = NULL;
    int ret = 0, total_get = 0;
    int bytess = sizeof(fmt_t) * MIC_CHN;
    fmt_t buf[MIC_PERIOD_SIZE][MIC_CHN] = {0};
    fmt_t buf_out[LINE_PERIOD_SIZE][LINE_AO_CHN] = {0};
    fmt_t buftmp[LINE_PERIOD_SIZE] = {0};
    snd_pcm_sframes_t avail;
    int bytess_buftmp = sizeof(buftmp);
    int start_play = 0;
    int play_delay = 0;

    prctl(PR_SET_NAME, g_mic_task_state.name);
    logi("---- proc %s start ----\n", g_mic_task_state.name);

    ret = mic_pcm_init(&pcm);
    if(ret < 0){
        loge("%s init  pcm error\n", g_mic_task_state.name);
        task_destroy(&g_mic_task_state);
    }

    ret = line_ao_pcm_init(&pcm_play);
    if(ret < 0){
        loge("%s init line-out pcm error\n", g_mic_task_state.name);
        task_destroy(&g_mic_task_state);
    }

    pcm_start(pcm, g_mic_card_id);
    enable_aec_out();
    enable_spk_out();

    while(g_mic_task_state.running){
        avail = snd_pcm_avail_update(pcm);
        if(avail < 0){
            #if 0
            if(avail == -EPIPE){
                ret = snd_pcm_prepare(pcm);
                if(ret < 0){
                    logw("MIC Can't recovery from xrun state\n");
                }
                delay_ms(5);
                snd_pcm_start(pcm);
            }
            else if(avail == -EIO){
                logw("MIC I/O Error\n");
                delay_s(1);
            }
            else{
                snd_pcm_recover(pcm, avail, 0);
            }
            #else
            check_and_recover_pcm(pcm, avail, "MIC");
            #endif
        }
        else if(avail >= MIC_PERIOD_SIZE){
            ret = snd_pcm_readi(pcm, buf, MIC_PERIOD_SIZE);
            if(ret > 0){
                for (int i = 0; i < algo_max; i++){
                    // if (ringbuf_get_free(g_ringbuf_mic[i]) < (bytess * ret))
                    // {
                    //     ringbuf_discard(g_ringbuf_mic[i], (bytess * ret));
                    // }
                    ringbuf_write(g_ringbuf_mic[i], buf, (bytess * ret));
                }

                total_get += ret;
            }
            else if(ret < 0){
                #if 0
                if(ret == -EPIPE){
                    ret = snd_pcm_prepare(pcm);
                    if(ret < 0){
                        logw("MIC Can't recovery from xrun state\n");
                    }
                    delay_ms(5);
                    snd_pcm_start(pcm);
                }
                else if(ret == -EIO){
                    logw("MIC I/O Error\n");
                    delay_s(1);
                }
                else{
                    snd_pcm_recover(pcm, ret, 0);
                }
                #else
                check_and_recover_pcm(pcm, ret, "MIC");
                #endif
            }

            if(total_get >= ALGO_PERIOD_SIZE){
                for (int i = 0; i < algo_max; i++){
                    sem_post(&g_sem_algo[i]);
                }
                total_get -= ALGO_PERIOD_SIZE;
            }
        }

        avail = snd_pcm_avail_update(pcm_play);
        if(avail < 0){
            check_and_recover_pcm(pcm_play, avail, "LINEAO");
        }
        else if(avail >= LINE_PERIOD_SIZE){
            if(ringbuf_get_used(g_ringbuf_line_ao) >= bytess_buftmp){
                ringbuf_read(g_ringbuf_line_ao, buftmp, bytess_buftmp);
            }

            for (int i = 0; i < LINE_PERIOD_SIZE; i++){
                buf_out[i][0] = buftmp[i];
                buf_out[i][1] = buftmp[i];
            }

            ret = snd_pcm_writei(pcm_play, buf_out, LINE_PERIOD_SIZE);
            if(ret < 0){
                check_and_recover_pcm(pcm_play, ret, "LINEAO");
            }
        }

        delay_us(100);
    }

    safe_cap_pcm_close(&pcm);
    safe_play_pcm_close(&pcm_play);

    disable_aec_out();
    disable_spk_out();

    logi("---- proc %s stop ----\n", g_mic_task_state.name);
    return 0;
}

static int line_ai_task(void *arg)
{
    snd_pcm_t *pcm = NULL;
    int ret = 0;
    fmt_t buf[LINE_PERIOD_SIZE][LINE_AI_CHN] = {0};
    fmt_t buftmp[LINE_PERIOD_SIZE] = {0};
    snd_pcm_sframes_t avail;
    int bytess = sizeof(fmt_t);
    int bytess_buftmp = sizeof(buftmp);

    prctl(PR_SET_NAME, g_line_ai_task_state.name);
    logi("---- proc %s start ----\n", g_line_ai_task_state.name);

    ret = line_ref_pcm_init(&pcm);
    if(ret < 0){
        loge("%s init  pcm error\n", g_line_ai_task_state.name);
        task_destroy(&g_line_ai_task_state);
    }

    pcm_start(pcm, g_line_card_id);

    while(1){
        if(snd_pcm_state(pcm) == SND_PCM_STATE_RUNNING){
            g_line_capture_start = 1;
            break;
        }
        delay_ms(1);
    }

    while(g_line_ai_task_state.running){
        avail = snd_pcm_avail_update(pcm);
        if(avail < 0){
            #if 0
            if(avail == -EPIPE){
                ret = snd_pcm_prepare(pcm);
                if(ret < 0){
                    logw("LINEAI Can't recovery from xrun state\n");
                }
                delay_ms(5);
                snd_pcm_start(pcm);
            }
            else if(avail == -EIO){
                logw("LINEAI I/O Error\n");
                delay_s(1);
            }
            else{
                snd_pcm_recover(pcm, avail, 0);
            }
            #else
            check_and_recover_pcm(pcm, avail, "LINEAI");
            #endif
        }
        else if(avail >= LINE_PERIOD_SIZE){
            ret = snd_pcm_readi(pcm, buf, LINE_PERIOD_SIZE);
            if(ret > 0){
                memset(buftmp, 0, bytess_buftmp);
                for(int i = 0; i< ret; i++){
                    buftmp[i] = buf[i][0];
                }
                for (int i = 0; i < algo_max; i++){
                    // if (ringbuf_get_free(g_ringbuf_line_ref[i]) < (bytess * ret))
                    // {
                    //     ringbuf_discard(g_ringbuf_line_ref[i], (bytess * ret));
                    // }
                    ringbuf_write(g_ringbuf_line_ref[i], buftmp, (bytess * ret));
                }
            }
            else if(ret < 0){
                #if 0
                if(ret == -EPIPE){
                    ret = snd_pcm_prepare(pcm);
                    if(ret < 0){
                        logw("LINEAI Can't recovery from xrun state\n");
                    }
                    delay_ms(5);
                    snd_pcm_start(pcm);
                }
                else if(ret == -EIO){
                    logw("LINEAI I/O Error\n");
                    delay_s(1);
                }
                else{
                    snd_pcm_recover(pcm, ret, 0);
                }
                #else
                check_and_recover_pcm(pcm, ret, "LINEAI");
                #endif
            }
        }

        delay_us(100);
    }

    safe_cap_pcm_close(&pcm);

    logi("---- proc %s stop ----\n", g_line_ai_task_state.name);
    return 0;
}

#if EN_UAC
static int uac_ao_task(void *arg)
{
#define RETRY_CNT_MAX   20
#define RECONNECT_DELAY_MAX 5
    snd_pcm_t *pcm = NULL;
    int ret = 0, retry = 0;
    fmt_t buf[UAC_PERIOD_SIZE][UAC_AO_CHN] = {0};
    fmt_t buftmp[UAC_PERIOD_SIZE] = {0};
    int bytess_buf = sizeof(buf);
    int bytess_buftmp = sizeof(buftmp);
    snd_pcm_sframes_t avail;
    int last_usb_state = 0, cur_usb_state = 0;
    int reconnect_delay = 0;

    prctl(PR_SET_NAME, g_uac_ao_task_state.name);
    logi("---- proc %s start ----\n", g_uac_ao_task_state.name);

    while(1){
        if(ringbuf_get_used(g_ringbuf_uac_ao) >= bytess_buftmp){
            break;
        }
        delay_ms(1);
    }

    ret = uac_ao_pcm_init(&pcm);
    while(ret != 0 && retry < RETRY_CNT_MAX){
        delay_s(1);
        ret = uac_ao_pcm_init(&pcm);
        if(ret == 0){
            loge("%s retry init  pcm success\n", g_uac_ao_task_state.name);
            break;
        }
        retry++;
    }
    if(ret < 0){
        loge("%s init  pcm error\n", g_uac_ao_task_state.name);
        task_destroy(&g_uac_ao_task_state);
    }

    while(1){
        if(ringbuf_get_used(g_ringbuf_uac_ao) >= bytess_buftmp){
            break;
        }

        ret = snd_pcm_writei(pcm, buf, UAC_PERIOD_SIZE);
        if(ret < 0){
            logd("UAO error(%s)\n", snd_strerror(ret));
            ret = check_and_recover_pcm(pcm, ret, "UAO");
            #if EN_USB_HOTPLUG
            if(ret == -EIO || ret == -EBADFD){
                logd("UAO I/O Error\n");
                safe_play_pcm_close(&pcm);
                reconnect_delay = RECONNECT_DELAY_MAX;
                break;
            }
            #endif
        }
        delay_us(100);
    }

    while(g_uac_ao_task_state.running){
        #if EN_USB_HOTPLUG
        cur_usb_state = (check_usb_connect() && check_usb_configured()) ? 1 : 0;
        if(cur_usb_state != last_usb_state){
            if(!cur_usb_state && pcm){
                logd("usb disconnect\n");
                safe_play_pcm_close(&pcm);
                reconnect_delay = RECONNECT_DELAY_MAX;
            }
            last_usb_state = cur_usb_state;
        }

        if(!pcm){
            if(reconnect_delay > 0){
                delay_s(1);
                reconnect_delay -= 1;
                continue;
            }

            cur_usb_state = (check_usb_connect() && check_usb_configured()) ? 1 : 0;
            last_usb_state = cur_usb_state;
            if(cur_usb_state){
                logd("usb reconnect\n");
                retry = 0;
                ret = uac_ao_pcm_init(&pcm);
                while((ret != 0) && (retry < RETRY_CNT_MAX) && g_uac_ao_task_state.running){
                    delay_s(1);
                    ret = uac_ao_pcm_init(&pcm);
                    if(ret == 0){
                        loge("%s retry init  pcm success\n", g_uac_ao_task_state.name);
                        break;
                    }
                    retry++;

                    cur_usb_state = (check_usb_connect() && check_usb_configured()) ? 1 : 0;
                    last_usb_state = cur_usb_state;
                    if(!cur_usb_state){
                        logd("usb disconnected, stop reconnect\n");
                        break;
                    }
                }
            }
            else{
                delay_s(1);
                continue;
            }
        }
        #endif

        if(pcm){
            avail = snd_pcm_avail_update(pcm);
            if(avail < 0){
                #if EN_USB_HOTPLUG
                ret = check_and_recover_pcm(pcm, avail, "UAO");
                if(ret == -EIO || ret == -EBADFD){
                    safe_play_pcm_close(&pcm);
                }
                #else
                if(avail == -EPIPE){
                    ret = snd_pcm_prepare(pcm);
                    if(ret < 0){
                        logw("UAO Can't recovery from xrun state\n");
                    }
                    delay_ms(5);
                    snd_pcm_start(pcm);
                }
                else if(avail == -EIO){
                    logw("UAO I/O Error\n");
                    delay_s(1);
                }
                else{
                    snd_pcm_recover(pcm, avail, 0);
                }
                #endif
            }
            else if(avail >= UAC_PERIOD_SIZE){
                memset(buf, 0, bytess_buf);
                memset(buftmp, 0, bytess_buftmp);
                if(ringbuf_get_used(g_ringbuf_uac_ao) >= bytess_buftmp){
                    ringbuf_read(g_ringbuf_uac_ao, buftmp, bytess_buftmp);
                }
                for (int i = 0; i < UAC_PERIOD_SIZE; i++){
                    buf[i][0] = buftmp[i];
                    buf[i][1] = buftmp[i];
                }

                ret = snd_pcm_writei(pcm, buf, UAC_PERIOD_SIZE);
                if(ret < 0){
                    #if EN_USB_HOTPLUG
                    ret = check_and_recover_pcm(pcm, ret, "UAO");
                    if(ret == -EIO || ret == -EBADFD){
                        safe_play_pcm_close(&pcm);
                    }                   
                    #else
                    if(ret == -EPIPE){
                        ret = snd_pcm_prepare(pcm);
                        if(ret < 0){
                            logw("UAO Can't recovery from xrun state\n");
                        }
                        delay_ms(5);
                        snd_pcm_start(pcm);
                    }
                    else if(ret == -EIO){
                        logw("UAO I/O Error\n");
                        delay_s(1);
                    }
                    else{
                        snd_pcm_recover(pcm, ret, 0);
                    }
                    #endif
                }
            }
        }

        delay_us(100);
    }

    safe_play_pcm_close(&pcm);

    logi("---- proc %s stop ----\n", g_uac_ao_task_state.name);
    return 0;
}
#endif

static void smooth_data(fmt_t *data, int len)
{ 
    for (int i = 0; i < len; i++){
        int source_idx = i % len;
        float progress = (float)i / len;
        float factor = (1.0f + cosf(progress * M_PI)) * 0.5f;
        data[i] = (fmt_t)(data[source_idx] * factor);
    } 
}
#define SYSTEM_STABLE_TIME  120 //s
static int line_ao_task(void *arg)
{
    snd_pcm_t *pcm = NULL;
    int ret = 0;
    fmt_t buf[LINE_PERIOD_SIZE][LINE_AO_CHN] = {0};
    fmt_t buftmp[LINE_PERIOD_SIZE] = {0};
    int bytess_buf = sizeof(buf);
    int bytess_buftmp = sizeof(buftmp);
    int bytess = sizeof(fmt_t);
    int try_fill = 0;
    snd_pcm_sframes_t avail;
    time_t start_time, cur_time;

    prctl(PR_SET_NAME, g_line_ao_task_state.name);
    logi("---- proc %s start ----\n", g_line_ao_task_state.name);

    while(!g_line_capture_start){
        delay_ms(1);
    }
    while(1){
        if(ringbuf_get_used(g_ringbuf_line_ao) >= bytess_buftmp){
            break;
        }
        //这里填充的数据不是从环缓存取出来的，但capture却会将这些数据0给到环缓存，这里循环的次数越多，增加的延迟越大
        // snd_pcm_writei(pcm, buf, LINE_PERIOD_SIZE);
        delay_us(100);
    }
    ret = line_ao_pcm_init(&pcm);
    if(ret < 0){
        loge("%s init  pcm error\n", g_line_ao_task_state.name);
        task_destroy(&g_line_ao_task_state);
    }

    enable_aec_out();
    enable_spk_out();
    start_time = time(NULL);

    while(g_line_ao_task_state.running){
        avail = snd_pcm_avail_update(pcm);
        if(avail < 0){
            #if 0
            if(avail == -EPIPE){
                ret = snd_pcm_prepare(pcm);
                if(ret < 0){
                    logw("LINEAO Can't recovery from xrun state\n");
                }
                delay_ms(5);
                snd_pcm_start(pcm);
            }
            else if(avail == -EIO){
                logw("LINEAO I/O Error\n");
                delay_s(1);
            }
            else{
                snd_pcm_recover(pcm, avail, 0);
            }
            #else
            check_and_recover_pcm(pcm, avail, "LINEAO");
            #endif
        }
        else if(avail >= LINE_PERIOD_SIZE){
            if(ringbuf_get_used(g_ringbuf_line_ao) >= bytess_buftmp){
                ringbuf_read(g_ringbuf_line_ao, buftmp, bytess_buftmp);
            }
            else{
                ringbuf_read_try(g_ringbuf_fill_data, buftmp, bytess_buftmp);
                logd("put fill data(%d)\n", try_fill);
            }

            for (int i = 0; i < LINE_PERIOD_SIZE; i++){
                buf[i][0] = buftmp[i];
                buf[i][1] = buftmp[i];
            }

            ret = snd_pcm_writei(pcm, buf, LINE_PERIOD_SIZE);
            if(ret < 0){
                #if 0
                if(ret == -EPIPE){
                    ret = snd_pcm_prepare(pcm);
                    if(ret < 0){
                        logw("LINEAO Can't recovery from xrun state\n");
                    }
                    delay_ms(5);
                    snd_pcm_start(pcm);
                }
                else if(ret == -EIO){
                    logw("LINEAO I/O Error\n");
                    delay_s(1);
                }
                else{
                    snd_pcm_recover(pcm, ret, 0);
                }
                #else
                check_and_recover_pcm(pcm, ret, "LINEAO");
                #endif
            }
        }

        delay_us(100);
    }

    disable_aec_out();
    disable_spk_out();

    safe_play_pcm_close(&pcm);

    logi("---- proc %s stop ----\n", g_line_ao_task_state.name);
    return 0;
}

static int algo1_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0, last_cost_max = 0;
    int algo_mic_idx[MIC_CNT] = {2, 6, 4, 5, 3, 1, 0};
    fmt_t rec[ALGO_PERIOD_SIZE][REC_CHN] = {0};
    fmt_t micin[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    fmt_t mic[MIC_CNT][ALGO_PERIOD_SIZE] = {0};
    fmt_t out[ALGO_PERIOD_SIZE] = {0};
    fmt_t ref[ALGO_PERIOD_SIZE] = {0};
    int bytess_mic = sizeof(micin);
    int bytess_ref = sizeof(ref);
    int bytess_out = sizeof(out);
    int bytess_rec = sizeof(rec);
    int bytess = sizeof(fmt_t);
    int used_len, discard_len;

    prctl(PR_SET_NAME, g_algo1_task_state.name);
    logi("---- proc %s start ----\n", g_algo1_task_state.name);

    gettimeofday(&tvlast, NULL);
    while(g_algo1_task_state.running){
        sem_wait(&g_sem_algo[algo_1]);

        if(ringbuf_get_used(g_ringbuf_mic[algo_1]) < bytess_mic){
            delay_us(100);
            continue;
        }

        ringbuf_read(g_ringbuf_mic[algo_1], micin, bytess_mic);
        for (int i = 0; i < MIC_CNT; i++){
            for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic[i][j] = micin[j][algo_mic_idx[i]];
            }
        }

        if(ringbuf_get_used(g_ringbuf_line_ref[algo_1]) >= bytess_ref){
            ringbuf_read(g_ringbuf_line_ref[algo_1], ref, bytess_ref);
        }

        gettimeofday(&tvbef, NULL);
#if EN_ALGO
        _algo_process1((short *)mic, (short *)out, (short *)ref);
#else
        for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
            out[j] = mic[0][j] + mic[1][j] + mic[2][j] + mic[3][j] + mic[4][j] + mic[5][j] + mic[6][j];
        }
#endif
        gettimeofday(&tvaft, NULL);
        
        // if(ringbuf_get_free(g_ringbuf_fill_data) < bytess_out){
        //     ringbuf_discard(g_ringbuf_fill_data, bytess_out);
        // }
        // ringbuf_write(g_ringbuf_fill_data, out, bytess_out);
        
#if EN_UAC
        if(ringbuf_get_free(g_ringbuf_uac_ao) < bytess_out){
            ringbuf_discard(g_ringbuf_uac_ao, bytess_out);
        }
        ringbuf_write(g_ringbuf_uac_ao, out, bytess_out);
#endif

        // if(ringbuf_get_free(g_ringbuf_line_ao) < bytess_out){
        //     ringbuf_discard(g_ringbuf_line_ao, bytess_out);
        // }
        ringbuf_write(g_ringbuf_line_ao, out, bytess_out);

        cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALGO_TIME_MS){
            time_out_cnt++;
            // if(cost_max > last_cost_max){
            //     double delay = cost_max - ALGO_TIME_MS;
            //     int delay_sample = (int)round(delay * SAMPLE_PER_MS_FLOAT);//四舍五入
            //     int bytess_delay = bytess * delay_sample;
            //     last_cost_max = cost_max;
            //     if(ringbuf_get_free(g_ringbuf_line_ao) < bytess_delay){
            //         ringbuf_discard(g_ringbuf_line_ao, bytess_delay);
            //     }
            //     ringbuf_write(g_ringbuf_line_ao, out, bytess_delay);
            // }
        }
        gettimeofday(&tvcur, NULL);
        if(check_time_increment_s(tvlast, tvcur) >= 3){
            gettimeofday(&tvlast, NULL);
            if(time_out_cnt > 0){
                logd("----------algo1 cost time err max=%.2f ms, voer_time_count=%ld\n", 
                    cost_max, time_out_cnt);
            }
            else{
                logd("----------algo1 cost time max=%.2f ms\n", cost_max);
            }
            time_out_cnt = 0;
            cost_max = 0;
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            for (int i = 0; i < MIC_CNT; i++){
                for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
                    rec[j][i] = micin[j][algo_mic_idx[i]];
                }
            }
            for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
                rec[j][rec_ref] = ref[j];
                rec[j][rec_out] = out[j];
            }

            // if(ringbuf_get_free(g_ringbuf_rec) >= bytess_rec){
                ringbuf_write(g_ringbuf_rec, rec, bytess_rec);
            // }
        }

        delay_us(100);
    }

    logi("---- proc %s stop ----\n", g_algo1_task_state.name);
    return 0;
}

static int algo2_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0;
    fmt_t micin[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    fmt_t mic[MIC_CNT][ALGO_PERIOD_SIZE] = {0};
    fmt_t ref[ALGO_PERIOD_SIZE] = {0};
    int bytess_mic = sizeof(mic);
    int bytess_ref = sizeof(ref);

    prctl(PR_SET_NAME, g_algo2_task_state.name);
    logi("---- proc %s start ----\n", g_algo2_task_state.name);

    gettimeofday(&tvlast, NULL);
    while(g_algo2_task_state.running){
        sem_wait(&g_sem_algo[algo_2]);

        if(ringbuf_get_used(g_ringbuf_mic[algo_2]) < bytess_mic){
            delay_us(100);
            continue;
        }

        ringbuf_read(g_ringbuf_mic[algo_2], micin, bytess_mic);
        for (int i = 0; i < MIC_CNT; i++){
            for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic[i][j] = micin[j][i];
            }
        }

        if(ringbuf_get_used(g_ringbuf_line_ref[algo_2]) >= bytess_ref){
            ringbuf_read(g_ringbuf_line_ref[algo_2], ref, bytess_ref);
        }

        gettimeofday(&tvbef, NULL);
#if EN_ALGO
        _algo_process2((short *)mic, (short *)ref);
#endif
        gettimeofday(&tvaft, NULL);
        
        cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALGO_TIME_MS){
            time_out_cnt++;
        }
        gettimeofday(&tvcur, NULL);
        if(check_time_increment_s(tvlast, tvcur) >= 3){
            gettimeofday(&tvlast, NULL);
            if(time_out_cnt > 0){
                logd("----------algo2 cost time err max=%.2f ms, voer_time_count=%ld\n", 
                    cost_max, time_out_cnt);
            }
            else{
                logd("----------algo2 cost time max=%.2f ms\n", cost_max);
            }
            time_out_cnt = 0;
            cost_max = 0;
        }

        delay_us(100);
    }

    logi("---- proc %s stop ----\n", g_algo2_task_state.name);
    return 0;
}

static int algo3_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0;
    fmt_t micin[ALGO_PERIOD_SIZE][MIC_CHN] = {0};
    fmt_t mic[MIC_CNT][ALGO_PERIOD_SIZE] = {0};
    fmt_t ref[ALGO_PERIOD_SIZE] = {0};
    int bytess_mic = sizeof(mic);
    int bytess_ref = sizeof(ref);

    prctl(PR_SET_NAME, g_algo3_task_state.name);
    logi("---- proc %s start ----\n", g_algo3_task_state.name);

    gettimeofday(&tvlast, NULL);
    while(g_algo3_task_state.running){
        sem_wait(&g_sem_algo[algo_3]);

        if(ringbuf_get_used(g_ringbuf_mic[algo_3]) < bytess_mic){
            delay_us(100);
            continue;
        }

        ringbuf_read(g_ringbuf_mic[algo_3], micin, bytess_mic);
        for (int i = 0; i < MIC_CNT; i++){
            for (int j = 0; j < ALGO_PERIOD_SIZE; j++){
                mic[i][j] = micin[j][i];
            }
        }

        if(ringbuf_get_used(g_ringbuf_line_ref[algo_3]) >= bytess_ref){
            ringbuf_read(g_ringbuf_line_ref[algo_3], ref, bytess_ref);
        }

        gettimeofday(&tvbef, NULL);
#if EN_ALGO
        _algo_process3((short *)mic, (short *)ref);
#endif
        gettimeofday(&tvaft, NULL);
        
        cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALGO_TIME_MS){
            time_out_cnt++;
        }
        gettimeofday(&tvcur, NULL);
        if(check_time_increment_s(tvlast, tvcur) >= 3){
            gettimeofday(&tvlast, NULL);
            if(time_out_cnt > 0){
                logd("----------algo3 cost time err max=%.2f ms, voer_time_count=%ld\n", 
                    cost_max, time_out_cnt);
            }
            else{
                logd("----------algo3 cost time max=%.2f ms\n", cost_max);
            }
            time_out_cnt = 0;
            cost_max = 0;
        }

        delay_us(100);
    }

    logi("---- proc %s stop ----\n", g_algo3_task_state.name);
    return 0;
}

static void rec_start(void)
{
	char path[256] = {0};

    snprintf(path, sizeof(path), "/userdata/rec.wav");
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

static int rec_task(void *arg)
{
    udp_server* recv_udp = NULL;
    int wcnt,ret,poll_ms;
    fmt_t wrbuff[SAMPLE_RATE][REC_CHN] = {0};
    int bytes_wrbuff = sizeof(wrbuff);
    char buf[1024] = {0};
    struct sockaddr_in start_client_addr;
    struct sockaddr_in client_addr;
    unsigned long rec_time;
    struct timeval tvcur, tvstart;

    prctl(PR_SET_NAME, g_rec_task_state.name);
    logi("---- proc %s start ----\n", g_rec_task_state.name);

    recv_udp = udp_server_init(UDP_SERVER_PORT);
    if(!recv_udp){
        loge("record recv udp init error\n");
        exit(-1);
    }
    poll_ms = 1000;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    while(g_rec_task_state.running){
        ret = udp_server_recv(recv_udp, &client_addr, buf, sizeof(buf), poll_ms);
        if(ret > 0){
            buf[sizeof(buf) - 1] = '\0';
            logd("recv cmmd: %s\n", buf);
            if (strstr(buf, RECORD_CMD_START)) {
                gettimeofday(&tvstart, NULL);
                rec_start();
                ringbuf_cleanup(g_ringbuf_rec);
                atomic_store(&g_record_action, algo_record_cmd_start);
                udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                memcpy(&start_client_addr, &client_addr, sizeof(struct sockaddr_in));
                logd("send cmd: %s, ret:%d\n", buf, ret);
            } else if (strstr(buf, RECORD_CMD_STOP)) {
                atomic_store(&g_record_action, algo_record_cmd_stop);
                ret = udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                logd("send cmd: %s, ret:%d\n", buf, ret);
                if((client_addr.sin_addr.s_addr != start_client_addr.sin_addr.s_addr) ||
                    (client_addr.sin_port != start_client_addr.sin_port)){
                    logd("------ diff client addr ------\n");
                    memcpy(&client_addr, &start_client_addr, sizeof(struct sockaddr_in));
                }
            } else if (strstr(buf, RECORD_CMD_STATUS)) {
                if(atomic_load(&g_record_action) == algo_record_cmd_start)  {
                    gettimeofday(&tvcur, NULL);
                    rec_time = check_time_increment_s(tvstart, tvcur);
                    logd("rec_time=%ld s\n", rec_time);
                    if(rec_time < MAX_REC_TIME){
                        snprintf(buf, sizeof(buf), "%s=%s %lds", RECORD_CMD_STATUS, "going", rec_time+1);
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        logd("send cmd: %s, ret:%d\n", buf, ret);
                    }
                    else if(rec_time+1 >= MAX_REC_TIME){
                        snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "end_of_record");
                        udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
                        logd("to end of recording\n");
                    }
                }
            }
        }

        if(atomic_load(&g_record_action) == algo_record_cmd_start){
            if(ringbuf_get_used(g_ringbuf_rec) >= bytes_wrbuff){
                ringbuf_read(g_ringbuf_rec, wrbuff, bytes_wrbuff);
                if(p_file_rec){
                    wcnt = fwrite(wrbuff, 1, bytes_wrbuff, p_file_rec);
                    total_size += wcnt;
                    logd("record write data total_size=%ld\n", total_size);
                }
            }
        }
        else if(atomic_load(&g_record_action) == algo_record_cmd_stop){      
            atomic_store(&g_record_action, algo_record_cmd_none);
            rec_stop();
            logd("record complete, g_record_action=%d\n", atomic_load(&g_record_action));
            snprintf(buf, sizeof(buf), "%s=%s", RECORD_CMD_STATUS, "finish");
            udp_server_send(recv_udp, &client_addr, buf, sizeof(buf));
            logd("send cmd: %s, ret:%d\n", buf, ret);
        }

        delay_ms(10);
    }

    logi("---- proc %s stop ----\n", g_rec_task_state.name);
    return 0;
}

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

int audio_start()
{
    int ret = 0;
    int size = 0;

    char name[256] = {0};
    sprintf(name, "hw:%s", g_line_card_id);
    ti3104_ctrl(name);

    memset(name, 0, sizeof(name));
    sprintf(name, "hw:%s", g_mic_card_id);
    es7210_ctrl(name);

    algo_eq_init();
    // float eq_val[EQ_BAND] = {0.0};
    // check_eq(eq_val, EQ_BAND);
    // for(int i = 0; i < EQ_BAND; i++){
    //     logi("cur eq[%d]: %.2f dB\n", i+1, eq_val[i]);
    // }

    for (int i = 0; i < algo_max; i++){
        sem_init(&g_sem_algo[i], 0, 0);
    }

    if(MIC_PERIOD_SIZE >= ALGO_PERIOD_SIZE){
        size = sizeof(fmt_t) * MIC_CHN * ALGO_PERIOD_SIZE * 2;
    }
    else{
        size = sizeof(fmt_t) * MIC_CHN * MIC_PERIOD_SIZE * 4;
    }
    for (int i = 0; i < algo_max; i++){
        g_ringbuf_mic[i] = ringbuf_init(size, "mic");
    }

    if(LINE_PERIOD_SIZE >= ALGO_PERIOD_SIZE){
        size = sizeof(fmt_t) * ALGO_PERIOD_SIZE * 2;
    }
    else{
        size = sizeof(fmt_t) * LINE_PERIOD_SIZE * 4;
    }
    for (int i = 0; i < algo_max; i++){
        g_ringbuf_line_ref[i] = ringbuf_init(size, "line_ref");
    }

#if EN_UAC
    if(UAC_PERIOD_SIZE >= ALGO_PERIOD_SIZE){
        size = sizeof(fmt_t) * ALGO_PERIOD_SIZE * 2;
    }
    else{
        size = sizeof(fmt_t) * UAC_PERIOD_SIZE * 4;
    }
    g_ringbuf_uac_ao = ringbuf_init(size, "uac_ao");
#endif
    if(LINE_PERIOD_SIZE >= ALGO_PERIOD_SIZE){
        size = sizeof(fmt_t) * ALGO_PERIOD_SIZE * 2;
    }
    else{
        size = sizeof(fmt_t) * LINE_PERIOD_SIZE * 4;
    }
    g_ringbuf_line_ao = ringbuf_init(size, "line_ao");

    size = sizeof(fmt_t) * ALGO_PERIOD_SIZE;
    g_ringbuf_fill_data = ringbuf_init(size, "fill_data");

    size = sizeof(fmt_t) * REC_CHN * SAMPLE_RATE * 2;
    g_ringbuf_rec = ringbuf_init(size, "rec");

    ret = task_init(CPU1, 70, "algo1", &g_algo1_task_state, algo1_task);
    if(ret != 0){
        loge("create algo1 task error\n");
        goto algo1_error;
    }

    ret = task_init(CPU2, 70, "algo2", &g_algo2_task_state, algo2_task);
    if(ret != 0){
        loge("create algo2 task error\n");
        goto algo2_error;
    }

    ret = task_init(CPU3, 70, "algo3", &g_algo3_task_state, algo3_task);
    if(ret != 0){
        loge("create algo3 task error\n");
        goto algo3_error;
    }

    ret = task_init(CPU0, 70, "mic", &g_mic_task_state, mic_task);
    if(ret != 0){
        loge("create mic task error\n");
        goto mic_error;
    }

    ret = task_init(CPU0, 70, "line_ai", &g_line_ai_task_state, line_ai_task);
    if(ret != 0){
        loge("create line ai task error\n");
        goto line_ai_error;
    }
#if EN_UAC
    ret = task_init(CPU0, 60, "uac_ao", &g_uac_ao_task_state, uac_ao_task);
    if(ret != 0){
        loge("create uac ao task error\n");
        goto uao_error;
    }
#endif
    // ret = task_init(CPU0, 70, "line_ao", &g_line_ao_task_state, line_ao_task);
    // if(ret != 0){
    //     loge("create line ao task error\n");
    //     goto line_error;
    // }

    ret = task_init(CPU0, 60, "rec", &g_rec_task_state, rec_task);
    if(ret != 0){
        loge("create rec task error\n");
        goto rec_error;
    }

    ret = init_uevent_listen();
    if(ret != 0){
        loge("create uevent task error\n");
        goto event_error;
    }

    logi("---- audio start success ----\n");
    return 0;

    loge("---- audio start failed ----\n");
event_error:
    task_destroy(&g_rec_task_state);
rec_error:
//     task_destroy(&g_line_ao_task_state);
// line_error:
#if EN_UAC
    task_destroy(&g_uac_ao_task_state);
uao_error:
#endif
    task_destroy(&g_line_ai_task_state);
line_ai_error:
    task_destroy(&g_mic_task_state);
mic_error:
    task_destroy(&g_algo3_task_state);
algo3_error:
    task_destroy(&g_algo2_task_state);
algo2_error:
    task_destroy(&g_algo1_task_state);
algo1_error:
    for (int i = 0; i < algo_max; i++){
        sem_destroy(&g_sem_algo[i]);
        if(g_ringbuf_mic[i]){
            ringbuf_destroy(g_ringbuf_mic[i]);
        }
        if(g_ringbuf_line_ref[i]){
            ringbuf_destroy(g_ringbuf_line_ref[i]);
        }
    }
#if EN_UAC
    if(g_ringbuf_uac_ao){
        ringbuf_destroy(g_ringbuf_uac_ao);
    }
#endif
    if(g_ringbuf_line_ao){
        ringbuf_destroy(g_ringbuf_line_ao);
    }
    if(g_ringbuf_fill_data){
        ringbuf_destroy(g_ringbuf_fill_data);
    }
    if(g_ringbuf_rec){
        ringbuf_destroy(g_ringbuf_rec);
    }
    return -1;
}

void audio_stop()
{
    task_destroy(&g_line_ai_task_state);
    task_destroy(&g_rec_task_state);
    // task_destroy(&g_line_ao_task_state);
#if EN_UAC
    task_destroy(&g_uac_ao_task_state);
#endif
    task_destroy(&g_algo1_task_state);
    task_destroy(&g_algo2_task_state);
    task_destroy(&g_algo3_task_state);
    task_destroy(&g_mic_task_state);
    destroy_event_listen();

    for (int i = 0; i < algo_max; i++){
        if(g_ringbuf_mic[i]){
            ringbuf_destroy(g_ringbuf_mic[i]);
        }
        if(g_ringbuf_line_ref[i]){
            ringbuf_destroy(g_ringbuf_line_ref[i]);
        }
        sem_destroy(&g_sem_algo[i]);
    }
#if EN_UAC
    if(g_ringbuf_uac_ao){
        ringbuf_destroy(g_ringbuf_uac_ao);
    }
#endif
    if(g_ringbuf_line_ao){
        ringbuf_destroy(g_ringbuf_line_ao);
    }
    if(g_ringbuf_fill_data){
        ringbuf_destroy(g_ringbuf_fill_data);
    }
    if(g_ringbuf_rec){
        ringbuf_destroy(g_ringbuf_rec);
    }
}

#ifdef __cplusplus
}
#endif