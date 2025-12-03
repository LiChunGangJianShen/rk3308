#include <semaphore.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include <termios.h>
#include <stdbool.h>
#include "log.h"
#include "thread.h"
#include "alsa_api.h"
#include "ringbuffer.h"
#include "comm.h"
#include "udp_server.h"
#include "wav_file.h"
#include "API.h"
#include "audio.h"
#include "serial.h"
#include "rkgpio.h"

#define SAMPLE_RATE 22050
#define PERIOD_SIZE 48
#define BUFFER_SIZE PERIOD_SIZE*4
#define ALG_FRAMES  48
#define ALG_COST_TIME   2

//Asound Card Name
#define CAPTURE_CARD_NAME  "hw:h20aiao,0"//"default"   //default capture
#define PLAYBACK_CARD_NAME  "hw:h20aiao,0"//"default"   //default playback
//Channels
#define CAPTURE_CHN   2
#define PLAYBACK_CHN    2
#define ALGO_CHN    2
#define EQ_FILE "/data/eq.conf"

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
static sem_t g_sem_alg_ready1[alg_idx_max];

static struct ringbuffer *rngbuff_ao[PLAYBACK_CHN] = {0};
#define AO_RNGBUFF_SIZE BUFFER_SIZE

//RECORD
#define REC_RNGBUFF_SIZE SAMPLE_RATE*2
#define UDP_SERVER_PORT    (6000)
#define RECORD_CMD_START   "record-cmd-start"
#define RECORD_CMD_STOP    "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"
#define MAX_REC_TIME    60   //s

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
static pthread_state_t key_task_state;
static pthread_state_t serial_task_state;

#define FB_ON       1
#define FB_OFF      0
static int g_cur_fb_state = 1;
static int g_noise_state = 0;
// static const int g_level_eq[]={-12.0, -11.5, -11.0, -10.5, -};
static float g_eq[16] = {0.0};

static void rec_start(void);
static void rec_stop(void);

static void init_fb_gpio(void)
{
    rk_set_gpio_export(0, 'A', 5);
    rk_set_gpio_direction_in(0, 'A', 5);
}
static void destory_fb_gpio(void)
{
    rk_set_gpio_unexport(0, 'A', 5);
}
static int check_fb_state(void)
{
    return rk_get_gpio_value(0, 'A', 5);
}
static void init_mute_gpio(void)
{
    rk_set_gpio_export(0, 'A', 7);
    rk_set_gpio_direction_in(0, 'A', 7);
}
static void destory_mute_gpio(void)
{
    rk_set_gpio_unexport(0, 'A', 7);
}
static int check_mute_state(void)
{
    return rk_get_gpio_value(0, 'A', 7);
}
static int key_task(void *arg)
{
    prctl(PR_SET_NAME, "key_task");
    logi("key task start\n");

    int cur_mute=0, last_mute=0, already_mute=0;
    int cur_fb=0, last_fb=0;
    init_fb_gpio();
    init_mute_gpio();

    while(key_task_state.running){
        cur_fb = check_fb_state();
        if(cur_fb != last_fb){
            last_fb = cur_fb;
            if(!cur_fb){
                g_cur_fb_state = FB_ON;
            }
            else{
                g_cur_fb_state = FB_OFF;
            }
            logi("bypass state: %s\n", g_cur_fb_state?"off":"on");
            JDZH_FeedbackDestroy_FeedbackOnOff(g_cur_fb_state);
        }

        cur_mute = check_mute_state();
        if(cur_mute && !last_mute){
            if(!already_mute){
                JDZH_FeedbackDestroy_MuteOnOff(1);
            }
            else{
                JDZH_FeedbackDestroy_MuteOnOff(0);
            }

            already_mute = !already_mute;
            logi("mic state: %s\n", already_mute?"off":"on");
        }
        last_mute = cur_mute;

        usleep(10000);
    }

    destory_fb_gpio();
    destory_mute_gpio();

    logi("key task stop\n");
    return 0;
}

int key_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&key_task_state, 0, sizeof(key_task_state));
    ret  = create_thread("key_task", cpu, priority, key_task, &key_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "key_task");
    }

    return ret;
}

void key_task_exit(void)
{
    destroy_thread(&key_task_state);
}

float extract_float(const char *input)
{
    // 查找第一个 ": " 的位置
    const char *colon_space = strstr(input, ": ");
    if (colon_space == NULL) {
        fprintf(stderr, "未找到 ': ' 分隔符\n");
        return 0.0f;
    }
    
    // 定位子字符串起始位置（跳过 ": "）
    const char *start = colon_space + 2;
    
    // 查找下一个空格的位置（作为子字符串结束）
    const char *end = strchr(start, ' ');
    
    // 计算子字符串长度
    size_t length;
    if (end != NULL) {
        length = end - start;
    } else {
        length = strlen(start); // 若没有空格则到字符串末尾
    }
    
    // 复制子字符串到临时缓冲区
    char temp[256];
    if (length >= sizeof(temp) - 1) { // 防止溢出
        length = sizeof(temp) - 1;
    }
    strncpy(temp, start, length);
    temp[length] = '\0'; // 确保终止符
    
    // 转换为float
    char *endptr;
    errno = 0; // 清除错误标志
    float value = strtof(temp, &endptr);
    
    // 错误检查
    if (endptr == temp) {
        fprintf(stderr, "无法转换数字: '%s'\n", temp);
        return 0.0f;
    }
    if (errno == ERANGE) {
        perror("数值超出float范围");
        return 0.0f;
    }
    
    return value;
}
static int save_eq(float *eq, int len)
{
    int fd = 0;
    char buf[16][64] = {0};

    fd = open(EQ_FILE, O_CREAT|O_RDWR|O_TRUNC);
    if(fd < 0){
        loge("crate eq_file fail\n");
        return -1;
    }

    sprintf(buf[0], "EQ1: %.2f dB\n", g_eq[0]);
    sprintf(buf[1], "EQ2: %.2f dB\n", g_eq[1]);
    sprintf(buf[2], "EQ3: %.2f dB\n", g_eq[2]);
    sprintf(buf[3], "EQ4: %.2f dB\n", g_eq[3]);
    sprintf(buf[4], "EQ5: %.2f dB\n", g_eq[4]);
    sprintf(buf[5], "EQ6: %.2f dB\n", g_eq[5]);
    sprintf(buf[6], "EQ7: %.2f dB\n", g_eq[6]);
    sprintf(buf[7], "EQ8: %.2f dB\n", g_eq[7]);
    sprintf(buf[8], "EQ9: %.2f dB\n", g_eq[8]);
    sprintf(buf[9], "EQ10: %.2f dB\n", g_eq[9]);
    sprintf(buf[10], "EQ11: %.2f dB\n", g_eq[10]);
    sprintf(buf[11], "EQ12: %.2f dB\n", g_eq[11]);
    sprintf(buf[12], "EQ13: %.2f dB\n", g_eq[12]);
    sprintf(buf[13], "EQ14: %.2f dB\n", g_eq[13]);
    sprintf(buf[14], "EQ15: %.2f dB\n", g_eq[14]);
    sprintf(buf[15], "EQ16: %.2f dB\n", g_eq[15]);
    for(int i = 0; i < 16; i++){
        logi("%s\n", buf[i]);
        write(fd, buf[i], strlen(buf[i]));
    }

    return 0;
}
static void algo_eq_init(void)
{
    char buf[64] = {0};
    FILE *fp = NULL;
    int i=0;
    if(access(EQ_FILE, F_OK) == 0){
        fp = fopen(EQ_FILE, "rw+");
        if(!fp){
            loge("fopen %s fail\n", EQ_FILE);
            return;
        }
        while(fgets(buf, sizeof(buf), fp) != NULL){
            if(i < 16){
                g_eq[i] = extract_float(buf);
                i++;
            }
            else{
                loge("eq_file error\n");
                break;
            }
        }
        for(i = 0; i < 16; i++){
            logi("last eq: %.2f\n", g_eq[i]);
        }
        JDZH_FeedbackDestroy_SetEQ(g_eq);
        fclose(fp);
    }
    else{
        fp = fopen(EQ_FILE, "w");
        if(!fp){
            loge("fopen %s fail\n", EQ_FILE);
            return;
        }
        // memset(g_eq, 0.0, sizeof(g_eq));
        save_eq(g_eq, sizeof(g_eq)/sizeof(g_eq[0]));
        for(i = 0; i < 16; i++){
            logi("initial eq: %.2f\n", g_eq[i]);
        }
        JDZH_FeedbackDestroy_SetEQ(g_eq);
        fclose(fp);
    }
}
static void check_eq(char eq_buf[], int len)
{
    char buf[64] = {0};
    FILE *fp = NULL;
    int i=0;
    if(access(EQ_FILE, F_OK) == 0){
        fp = fopen(EQ_FILE, "r");
        if(!fp){
            loge("fopen %s fail", EQ_FILE);
            return;
        }
        while(fgets(buf, sizeof(buf), fp) != NULL){
            if(i < 16){
                g_eq[i] = extract_float(buf);
                i++;
            }
            else{
                loge("eq_file error\n");
                break;
            }
        }
        for(i = 0; i < 16 && i < len; i++){
            eq_buf[i] = 2*(g_eq[i] + 12);
            logi("get eq: %.2f dB, data: 0x%x\n", g_eq[i], eq_buf[i]);
        }
        fclose(fp);
    }
}

static bool check_sum(const char *buf, int length)
{
    int sum = 0, data_len=0;
    int i;

    data_len = buf[3];
    logi("data_len=%d", data_len);
    for(i = 1; i <= data_len+3; i++){
        sum += buf[i];
    }
    sum &= 0xff;
    logi("cal_sum=0x%x, recv_sum=0x%x, buf[%d]=0x%x\n", sum, buf[i], i, buf[i]);
    if(sum != buf[i]){
        return false;
    }

    return true;
}

static void serial_cmd_handle(int fd, char *buf, int data_len)
{
    int i,j;
    char data[16]={0};
    char eq_buf[16]={0};
    char sbuf[256]={0};
    int length = 16;
    int sum=0;
    if(buf == NULL || buf[0] != 0xFE || buf[data_len-1] != 0xFE || \
        (buf[2] != 0x01 && buf[2] != 0x02 && buf[2] != 0x03 && buf[2] != 0x04 && buf[2] != 0x05)){
        loge("invalid serial cmd\n");
        return;
    }

    if(buf[2] == 0x01){
        if(check_sum(buf, data_len) == false){
            loge("cmd handshake check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        sbuf[0] = 0xFE;
        sbuf[1] = 0x01;
        sbuf[2] = 0x01;
        sbuf[3] = 0x01;
        sbuf[4] = g_cur_fb_state?0x01:0x00;
        sbuf[5] = 0x00ff & (sbuf[1]+sbuf[2]+sbuf[3]+sbuf[4]);
        sbuf[6] = 0xFE;
        write(fd, sbuf, 7);
    }
    else if(buf[2] == 0x02){
        if(check_sum(buf, data_len) == false){
            loge("cmd set_eq check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        for(i = 0, j = 4; i < 16 && j < (data_len-2); i++, j++){
            if(buf[j] == 0x7d){
                if(buf[j+1] == 0x01){
                    data[i] = 0x7d;
                }
                else if(buf[j+1] == 0x02){
                    data[i] = 0xfe;
                }
                j++;
            }
            else{
                data[i] = buf[j];
            }
        }
        for(i = 0; i < 16; i++){
            g_eq[i] = (float)(data[i]*0.5-12);
            logi("eq para: %.2f db, data: 0x%x\n", g_eq[i], data[i]);
        }
        JDZH_FeedbackDestroy_SetEQ(g_eq);
        save_eq(g_eq, sizeof(g_eq)/sizeof(g_eq[0]));
        length=16;
        memset(sbuf, 0, sizeof(sbuf));
        for(i = 0; i < 16; i++){
            if(data[i] == 0x7d || data[i] == 0xfe){
                length++;
            }
        }
        sbuf[0] = 0xFE;
        sbuf[1] = 0x01;
        sbuf[2] = 0x02;
        sbuf[3] = length;
        j = 4;
        for(i = 0; i < 16; i++){
            if(data[i] == 0x7d){
                sbuf[j] = 0x7d;
                j++;
                sbuf[j] = 0x01;
            }
            else if(data[i] == 0xfe){
                sbuf[j] = 0x7d;
                j++;
                sbuf[j] = 0x02;
            }
            else{
                sbuf[j] = data[i];
            }

            j++;
        }
        for(i = 1; i < j; i++){
            sum += sbuf[i];
        }
        sum &= 0x00ff;
        sbuf[j] = sum;
        j++;
        sbuf[j] = 0xfe;

        write(fd, sbuf, j+1);
    }
    else if(buf[2] == 0x03){
        if(check_sum(buf, data_len) == false){
            loge("cmd fb_enable check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        #if 0
        if(0){
            if(buf[4] == 0x01){
                g_cur_fb_state = 1;
            }
            else{
                g_cur_fb_state = 0;
            }

            JDZH_FeedbackDestroy_FeedbackOnOff(g_cur_fb_state);
            memset(sbuf, 0, sizeof(sbuf));
            sbuf[0] = 0xFE;
            sbuf[1] = 0x01;
            sbuf[2] = 0x03;
            sbuf[3] = 0x01;
            sbuf[4] = g_cur_fb_state?0x01:0x00;
            sbuf[5] = sbuf[1]+sbuf[2]+sbuf[3]+sbuf[4];
            sbuf[6] = 0xFE;
            write(fd, sbuf, 7);
        }
        else{
            //如果按键FB_EN拉高了，则不响应串口设置反馈抑制状态命令，返回7个字节全0
            memset(sbuf, 0, sizeof(sbuf));
            write(fd, sbuf, 7);
        }
        #else
        //反馈抑制不接受串口命令控制，修改于2025-05-11
        memset(sbuf, 0, sizeof(sbuf));
        write(fd, sbuf, 7);
        #endif
    }
    else if(buf[2] == 0x04){
        if(check_sum(buf, data_len) == false){
            loge("cmd get_eq check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        check_eq(eq_buf, 16);
        length=16;
        memset(sbuf, 0, sizeof(sbuf));
        for(i = 0; i < 16; i++){
            if(eq_buf[i] == 0x7d || eq_buf[i] == 0xfe){
                length++;
            }
        }
        sbuf[0] = 0xFE;
        sbuf[1] = 0x01;
        sbuf[2] = 0x04;
        sbuf[3] = length;
        j = 4;
        for(i = 0; i < 16; i++){
            if(eq_buf[i] == 0x7d){
                sbuf[j] = 0x7d;
                j++;
                sbuf[j] = 0x01;
            }
            else if(eq_buf[i] == 0xfe){
                sbuf[j] = 0x7d;
                j++;
                sbuf[j] = 0x02;
            }
            else{
                sbuf[j] = eq_buf[i];
            }

            j++;
        }
        for(i = 1; i < j; i++){
            sum += sbuf[i];
        }
        sum &= 0x00ff;
        sbuf[j] = sum;
        j++;
        sbuf[j] = 0xfe;

        write(fd, sbuf, j+1);
    }
    else if(buf[2] == 0x05){
        if(check_sum(buf, data_len) == false){
            loge("cmd genrate noise check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        g_noise_state = buf[4];
        sbuf[0] = 0xFE;
        sbuf[1] = 0x01;
        sbuf[2] = 0x05;
        sbuf[3] = 0x01;
        sbuf[4] = g_noise_state?0x01:0x00;
        sbuf[5] = 0x00ff & (sbuf[1]+sbuf[2]+sbuf[3]+sbuf[4]);
        sbuf[6] = 0xFE;
        write(fd, sbuf, 7);
        JDZH_FeedbackDestroy_PinkNoiseOnOff(g_noise_state, 1.0);
    }
}
static int serial_task(void *arg)
{
#define SERIAL_DEV  "/dev/ttyS4"
#define SERIAL_BAUD_RATE    B115200
    prctl(PR_SET_NAME, "serial_task");
    logi("serial task start\n");

    int max_fd, ret=0;
    fd_set rfds;
    struct timeval tv;
    int serial_fd = -1;
    char rbuf[256]={0};

    serial_fd = serial_init(SERIAL_DEV, SERIAL_BAUD_RATE);
    max_fd = serial_fd;
    if(fcntl(serial_fd, F_GETFL) == -EBADF){
        loge("bad serial fd\n");
        goto __exit;
    }

    while(serial_task_state.running){
        FD_ZERO(&rfds);
        FD_SET(serial_fd, &rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ret = select(max_fd + 1, &rfds, NULL, NULL, &tv);
        if(ret > 0){
            if (FD_ISSET(serial_fd, &rfds)) {
                ret = read(serial_fd, rbuf, sizeof(rbuf));
                if(ret > 0){
                    for(int i = 0; i < ret; i++){
                        logi("ret=%d, recv: rbuf[%d]=0x%x\n", ret, i, 0xff & rbuf[i]);
                    }
                    serial_cmd_handle(serial_fd, rbuf, ret);
                }
                else if(ret == 0){
                    logi("no data recv\n");
                }
                else{
                    perror("read error\n");
                }
            }
        }
        else if(ret == 0){
            // logi("select serial fd timeout\n");
        }

        usleep(100);
    }

__exit:
    serial_exit(serial_fd);
    logi("serial task stop\n");
    return 0;
}

int serial_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&serial_task_state, 0, sizeof(serial_task_state));
    ret  = create_thread("serial_task", cpu, priority, serial_task, &serial_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "serial_task");
    }

    return ret;
}

void serial_task_exit(void)
{
    destroy_thread(&serial_task_state);
}

static int capture_task(void *arg)
{
    prctl(PR_SET_NAME, "capture_task");
    logi("capture task start\n");

    int err,get_frames=0;
    snd_pcm_t *ppcm_capture = NULL;
    alsa_api_para_t params_capture;
    audio_fmt_t capture_buff[PERIOD_SIZE][CAPTURE_CHN];
    snd_pcm_sframes_t avail_frames = 0;

    memset(capture_buff, 0, sizeof(capture_buff));

    // for(int i = 0; i < alg_idx_max; i++){
    //     sem_wait(&g_sem_alg_ready[i]);
    // }

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

    snd_pcm_start(ppcm_capture);

    while (capture_task_state.running)
    {
        if(ppcm_capture){
            avail_frames = snd_pcm_avail(ppcm_capture);
            if(avail_frames >= PERIOD_SIZE){
                memset(capture_buff, 0, sizeof(capture_buff));
                err = pcm_in(ppcm_capture, capture_buff, PERIOD_SIZE, "capture");
                if(err > 0){
                    get_frames += err;
                    for(int i = 0; i < alg_idx_max; i++){
                        if(rb_get_space_free(rngbuff_ai[i]) < sizeof(capture_buff)){
                            rb_discard(rngbuff_ai[i], sizeof(capture_buff));
                        }
                        rb_write(rngbuff_ai[i], capture_buff, sizeof(capture_buff));
                    }
                }
            }
            else if(avail_frames < 0){
                logd("err capture avail_frames=%ld,%s\n", avail_frames, snd_strerror(avail_frames));
                check_pcm_state(ppcm_capture, avail_frames, "capture");
            }
        }

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
    logi("capture task stop\n");

    return 0;
}

int capture_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&capture_task_state, 0, sizeof(capture_task_state));
    ret  = create_thread("capture_task", cpu, priority, capture_task, &capture_task_state);
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
    alsa_api_para_t params_playback;
    audio_fmt_t playback_buff[PERIOD_SIZE][PLAYBACK_CHN];
    audio_fmt_t tmpbuff[PERIOD_SIZE];
    snd_pcm_sframes_t avail_frames;

    // for(int i = 0; i < alg_idx_max; i++){
    //     sem_wait(&g_sem_alg_ready1[i]);
    // }
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

    while(rb_get_space_used(rngbuff_ao[0]) < sizeof(tmpbuff)){
        pcm_out(ppcm_playback, playback_buff, PERIOD_SIZE, "playback");
        usleep(100);
    }

    while (playback_task_state.running)
    {
        if(rb_get_space_used(rngbuff_ao[0]) < sizeof(tmpbuff)){
            usleep(100);
			continue;
        }
        
        for(int i = 0; i < PLAYBACK_CHN; i++){
            memset(tmpbuff, 0, sizeof(tmpbuff));
            rb_read(rngbuff_ao[i], tmpbuff, sizeof(tmpbuff));
            for(int j = 0; j < PERIOD_SIZE; j++){
                playback_buff[j][i] = tmpbuff[j];
            }
        }

        if(ppcm_playback){
            avail_frames = snd_pcm_avail(ppcm_playback);
            if(avail_frames >= PERIOD_SIZE){
                pcm_out(ppcm_playback, playback_buff, PERIOD_SIZE, "playback");
            }
			else if(avail_frames < 0){
                logd("err playback avail_frames=%ld,%s\n", avail_frames, snd_strerror(avail_frames));
                check_pcm_state(ppcm_playback, avail_frames, "playback");
            }
        }
        usleep(100);
    }
    
err_to_exit:
    if(ppcm_playback)
        snd_pcm_close(ppcm_playback);
    logi("playback task stop\n");
    return 0;
}

int playback_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&playback_task_state, 0, sizeof(playback_task_state));
    ret  = create_thread("playback_task", cpu, priority, playback_task, &playback_task_state);
    if(ret != 0){
        loge("create %s thread failed\n", "playback_task");
    }

    return ret;
}

void playback_task_exit(void)
{
    destroy_thread(&playback_task_state);
}

static int alg_task(void *arg)
{
    prctl(PR_SET_NAME, "alg_task");
    logi("alg task start\n");

    int i;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb,tvc,tvl;
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN];
    audio_fmt_t playback_buff[ALG_FRAMES][PLAYBACK_CHN];
    audio_fmt_t mic_data1[ALG_FRAMES];
    audio_fmt_t mic_data2[ALG_FRAMES];
	audio_fmt_t out_data1[ALG_FRAMES];
    audio_fmt_t out_data2[ALG_FRAMES];

    memset(capture_buff, 0, sizeof(capture_buff));
    memset(playback_buff, 0, sizeof(playback_buff));
    memset(mic_data1, 0, sizeof(mic_data1));
    memset(mic_data2, 0, sizeof(mic_data2));
    memset(out_data1, 0, sizeof(out_data1));
    memset(out_data2, 0, sizeof(out_data2));

    algo_eq_init();

    sem_post(&g_sem_alg_ready[alg_idx_1]);
    sem_post(&g_sem_alg_ready1[alg_idx_1]);

	gettimeofday(&tvc, NULL);
	gettimeofday(&tvl, NULL);
    while(alg_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_1]) < sizeof(capture_buff)){
            sem_wait(&g_sem_3a[alg_idx_1]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_1]) >= sizeof(capture_buff)){
            memset(capture_buff, 0, sizeof(capture_buff));
            rb_read(rngbuff_ai[alg_idx_1], capture_buff, sizeof(capture_buff));
            memset(mic_data1, 0, sizeof(mic_data1));
            memset(mic_data2, 0, sizeof(mic_data2));
            for(i = 0; i < ALG_FRAMES; i++){
                mic_data1[i] = capture_buff[i][0];
                mic_data2[i] = capture_buff[i][1];
            }

            gettimeofday(&tva, NULL);
            memset(out_data1, 0, sizeof(out_data1));
            memset(out_data2, 0, sizeof(out_data2));
#if ENABLE_ALGO
            JDZH_FeedbackDestroy_Process1(mic_data1, mic_data2, out_data1, out_data2, NULL, NULL);
#else
			memcpy(out_data1, mic_data1, sizeof(out_data1));
			memcpy(out_data2, mic_data2, sizeof(out_data2));
#endif
            gettimeofday(&tvb, NULL);
			gettimeofday(&tvc, NULL);
			
			if(check_time_increment_s(tvl, tvc) > 3){
				gettimeofday(&tvl, NULL);
				cost_time = check_time_increment_ms(tva, tvb);
				if(cost_time > ALG_COST_TIME){
					timeout_cnt++;
					logd("algo1 error, timeout (%ld)ms, timeout_cnt(%ld)\n", cost_time, timeout_cnt);
				}
				else{
				    logd("algo1 cost time (%ld)ms\n", cost_time);
				}
			}

            if(rb_get_space_free(rngbuff_ao[0]) < sizeof(out_data1)){
                rb_discard(rngbuff_ao[0], sizeof(out_data1));
            }
            rb_write(rngbuff_ao[0], out_data1, sizeof(out_data1));
#if TWO_OUT_DATA
            if(rb_get_space_free(rngbuff_ao[1]) < sizeof(out_data2)){
                rb_discard(rngbuff_ao[1], sizeof(out_data2));
            }
            rb_write(rngbuff_ao[1], out_data2, sizeof(out_data2));
#else
            if(rb_get_space_free(rngbuff_ao[1]) < sizeof(out_data1)){
                rb_discard(rngbuff_ao[1], sizeof(out_data1));
            }
            rb_write(rngbuff_ao[1], out_data1, sizeof(out_data1));
#endif

            if(atomic_load(&g_record_action) == algo_record_cmd_start){
                rb_write(rngbuff_rec, capture_buff, sizeof(capture_buff));
                for(i = 0; i < ALG_FRAMES; i++){
                    playback_buff[i][0] = out_data1[i];
#if TWO_OUT_DATA
                    playback_buff[i][1] = out_data2[i];
#else
                    playback_buff[i][1] = out_data1[i];
#endif
                }
                rb_write(rngbuff_rec_dataout, playback_buff, sizeof(playback_buff));
            }
        }

        usleep(100);
    }

    logi("alg task exit\n");
    return 0;
}

int alg_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg_task_state, 0, sizeof(alg_task_state));
    ret  = create_thread("alg_task", cpu, priority, alg_task, &alg_task_state);
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

    int i;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb,tvc,tvl;
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN];
    audio_fmt_t mic_data1[ALG_FRAMES];
    audio_fmt_t mic_data2[ALG_FRAMES];

    memset(capture_buff, 0, sizeof(capture_buff));
    memset(mic_data1, 0, sizeof(mic_data1));
    memset(mic_data2, 0, sizeof(mic_data2));

    sem_post(&g_sem_alg_ready[alg_idx_2]);
    sem_post(&g_sem_alg_ready1[alg_idx_2]); 

	gettimeofday(&tvc, NULL);
	gettimeofday(&tvl, NULL);
    while(alg2_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_2]) < sizeof(capture_buff)){
            sem_wait(&g_sem_3a[alg_idx_2]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_2]) >= sizeof(capture_buff)){
            memset(capture_buff, 0, sizeof(capture_buff));
            rb_read(rngbuff_ai[alg_idx_2], capture_buff, sizeof(capture_buff));
            memset(mic_data1, 0, sizeof(mic_data1));
            memset(mic_data2, 0, sizeof(mic_data2));
            for(i = 0; i < ALG_FRAMES; i++){
                mic_data1[i] = capture_buff[i][0];
                mic_data2[i] = capture_buff[i][1];
            }
            gettimeofday(&tva, NULL);
#if ENABLE_ALGO
            JDZH_FeedbackDestroy_Process2(mic_data1, mic_data2, NULL, NULL);
#endif
            gettimeofday(&tvb, NULL);
			gettimeofday(&tvc, NULL);

			if(check_time_increment_s(tvl, tvc) > 3){
				gettimeofday(&tvl, NULL);
				cost_time = check_time_increment_ms(tva, tvb);
				if(cost_time > ALG_COST_TIME){
					timeout_cnt++;
					logd("alg2 error, timeout (%ld)ms, timeout_cnt(%ld)\n", cost_time, timeout_cnt);
				}
				else{
				    logd("alg2 cost time (%ld)ms\n", cost_time);
				}
			}
        }

        usleep(100);
    }

    logi("alg2 task exit\n");
    return 0;
}

int alg2_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg2_task_state, 0, sizeof(alg2_task_state));
    ret  = create_thread("alg2_task", cpu, priority, alg2_task, &alg2_task_state);
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

    int i;
    unsigned long cost_time, timeout_cnt=0;
    struct timeval tva,tvb,tvc,tvl;   
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN];
    audio_fmt_t mic_data1[ALG_FRAMES];
    audio_fmt_t mic_data2[ALG_FRAMES];

    memset(capture_buff, 0, sizeof(capture_buff));
    memset(mic_data1, 0, sizeof(mic_data1));
    memset(mic_data2, 0, sizeof(mic_data2));

    sem_post(&g_sem_alg_ready[alg_idx_3]); 
    sem_post(&g_sem_alg_ready1[alg_idx_3]);

	gettimeofday(&tvc, NULL);
	gettimeofday(&tvl, NULL);
    while(alg2_task_state.running){
        if(rb_get_space_used(rngbuff_ai[alg_idx_3]) < sizeof(capture_buff)){
            sem_wait(&g_sem_3a[alg_idx_3]);
            continue;
        }

        if(rb_get_space_used(rngbuff_ai[alg_idx_3]) >= sizeof(capture_buff)){
            memset(capture_buff, 0, sizeof(capture_buff));
            rb_read(rngbuff_ai[alg_idx_3], capture_buff, sizeof(capture_buff));
            memset(mic_data1, 0, sizeof(mic_data1));
            memset(mic_data2, 0, sizeof(mic_data2));
            for(i = 0; i < ALG_FRAMES; i++){
                mic_data1[i] = capture_buff[i][0];
                mic_data2[i] = capture_buff[i][1];
            }

            gettimeofday(&tva, NULL);
#if ENABLE_ALGO
            JDZH_FeedbackDestroy_Process3(mic_data1, mic_data2, NULL, NULL);
#endif
            gettimeofday(&tvb, NULL);
			gettimeofday(&tvc, NULL);

			if(check_time_increment_s(tvl, tvc) > 3){
				gettimeofday(&tvl, NULL);
				cost_time = check_time_increment_ms(tva, tvb);
				if(cost_time > ALG_COST_TIME){
					timeout_cnt++;
					logd("alg3 error, timeout (%ld)ms, timeout_cnt(%ld)\n", cost_time, timeout_cnt);
				}
				else{
				    logd("alg3 cost time (%ld)ms\n", cost_time);
				}
			}
        }

        usleep(100);
    }

    logi("alg3 task exit\n");
    return 0;
}

int alg3_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&alg3_task_state, 0, sizeof(alg3_task_state));
    ret  = create_thread("alg3_task", cpu, priority, alg3_task, &alg3_task_state);
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
        wav_start_write(p_file_rec, &st_wavhead, 16, CAPTURE_CHN, SAMPLE_RATE);
    }

    snprintf(path, sizeof(path), "/data/algo_out.wav");
    if(!p_file_rec_dataout) {
        p_file_rec_dataout = fopen(path, "w");
        wav_start_write(p_file_rec_dataout, &st_wavhead_dataout, 16, PLAYBACK_CHN, SAMPLE_RATE);
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
    audio_fmt_t wrbuff[SAMPLE_RATE][CAPTURE_CHN];
    audio_fmt_t wrdataoutbuff[SAMPLE_RATE][PLAYBACK_CHN];
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

int rec_task_init(int cpu, int priority)
{
    int ret = 0;

    if(cpu < 0 || cpu > 3){
        logw("cpu num invalid(%d)\n", cpu);
        return -1;
    }

    memset(&rec_task_state, 0, sizeof(rec_task_state));
    ret  = create_thread("rec_task", cpu, priority, rec_task, &rec_task_state);
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
        sem_init(&g_sem_alg_ready1[i], 0, 0);
    }
}

void _sem_destroy(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        sem_destroy(&g_sem_3a[i]);
        sem_destroy(&g_sem_alg_ready[i]);
        sem_destroy(&g_sem_alg_ready1[i]);
    }
}

void init_rngbuff(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        rngbuff_ai[i] = rb_create(sizeof(audio_fmt_t)*AI_RNGBUFF_SIZE*CAPTURE_CHN);
    }
    for(int i = 0; i < PLAYBACK_CHN; i++){
        rngbuff_ao[i] = rb_create(sizeof(audio_fmt_t)*AO_RNGBUFF_SIZE*PLAYBACK_CHN);
    }
    rngbuff_rec = rb_create(sizeof(audio_fmt_t)*REC_RNGBUFF_SIZE*CAPTURE_CHN);
    rngbuff_rec_dataout = rb_create(sizeof(audio_fmt_t)*REC_RNGBUFF_SIZE*PLAYBACK_CHN);
}

void exit_rngbuff(void)
{
    for(int i = 0; i < alg_idx_max; i++){
        rb_destroy(rngbuff_ai[i]);
    }
    for(int i = 0; i < PLAYBACK_CHN; i++){
        rb_destroy(rngbuff_ao[i]);
    }
    rb_destroy(rngbuff_rec);
    rb_destroy(rngbuff_rec_dataout);
}
