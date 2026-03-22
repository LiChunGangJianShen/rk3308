#include <semaphore.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include <math.h>
#include <termios.h>
#include <stdbool.h>
#include "log.h"
#include "thread.h"
#include "alsa_api.h"
#include "ringbuffer.h"
#include "comm.h"
#include "udp_server.h"
#include "wav_file.h"
#include "algo.h"
#include "audio.h"
#include "serial.h"
#include "rkgpio.h"
#include "ad_ctrl.h"

#ifndef M_PI
#define M_PI        3.1415926
#endif
#define FB_ON       1
#define FB_OFF      0
#define UDP_SERVER_PORT     (6000)
#define RECORD_CMD_START    "record-cmd-start"
#define RECORD_CMD_STOP     "record-cmd-stop"
#define RECORD_CMD_STATUS   "record-cmd-status"
#define MAX_REC_TIME        60   //s
#define CAPTURE_CARD_NAME   "hw:h20aiao,0"//"default"   //default capture
#define PLAYBACK_CARD_NAME  "hw:h20aiao,0"//"default"   //default playback
#define EQ_FILE             "/data/eq.conf"

typedef enum{
    alg_idx_1=0,
    alg_idx_2,
    alg_idx_3,
    alg_idx_max
}algo_idx_t;
typedef enum {
    algo_record_cmd_none,
    algo_record_cmd_start,
    algo_record_cmd_stop,
}algo_record_cmd;

static sem_t g_sem_3a[alg_idx_max];
static struct ringbuffer *g_ringbuf_ai[alg_idx_max] = {0};
static struct ringbuffer *g_ringbuf_ao[PLAYBACK_CHN] = {0};
static struct ringbuffer *g_ringbuf_fill_data[PLAYBACK_CHN] = {0};
static struct ringbuffer *g_ringbuf_rec = 0;
static const float ALG_COST_TIME = (float)(ALG_FRAMES / (SAMPLE_RATE / 1000.0f));
static const float SAMPLE_PER_MS_FLOAT = (float)(SAMPLE_RATE / 1000.0f);
static FILE *p_file_rec = NULL;
struct my_wave_file_headers st_wavhead;
static unsigned long total_size = 0;
static atomic_int g_record_action = algo_record_cmd_none;

static pthread_state_t capture_task_state;
static pthread_state_t playback_task_state;
static pthread_state_t alg_task_state;
static pthread_state_t alg2_task_state;
static pthread_state_t alg3_task_state;
static pthread_state_t rec_task_state;
static pthread_state_t key_task_state;
static pthread_state_t serial_task_state;

static int g_cur_fb_state = 1;
static int g_noise_state = 0;
static int g_ai_enable = 0;
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
    int cur_mute=0, last_mute=0, already_mute=0;
    int cur_fb=0, last_fb=0;

    prctl(PR_SET_NAME, key_task_state.name);
    logi("---- proc %s start ----\n", key_task_state.name);

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
            feed_back_switch(g_cur_fb_state);
        }

        cur_mute = check_mute_state();
        if(cur_mute && !last_mute){
            if(!already_mute){
                feed_back_mute(1);
            }
            else{
                feed_back_mute(0);
            }

            already_mute = !already_mute;
            logi("mic state: %s\n", already_mute?"off":"on");
        }
        last_mute = cur_mute;

        delay_ms(10);
    }

    destory_fb_gpio();
    destory_mute_gpio();

    logi("---- proc %s stop ----\n", key_task_state.name);
    return 0;
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
        feed_back_set_eq(g_eq);
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
        feed_back_set_eq(g_eq);
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
    logd("cal_sum=0x%x, recv_sum=0x%x, buf[%d]=0x%x\n", sum, buf[i], i, buf[i]);
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
        (buf[2] != 0x01 && buf[2] != 0x02 && buf[2] != 0x03 && \
            buf[2] != 0x04 && buf[2] != 0x05 && buf[2] != 0x06)){
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
        feed_back_set_eq(g_eq);
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

            feed_back_switch(g_cur_fb_state);
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
        pinknoise_switch(g_noise_state, 1.0);
    }
    else if(buf[2] == 0x06){
        if(check_sum(buf, data_len) == false){
            loge("cmd ai wsitch check_sum error\n");
            memset(sbuf, 1, sizeof(sbuf));
            write(fd, sbuf, 7);
            return;
        }
        g_ai_enable = buf[4];
        sbuf[0] = 0xFE;
        sbuf[1] = 0x01;
        sbuf[2] = 0x06;
        sbuf[3] = 0x01;
        sbuf[4] = g_ai_enable?0x01:0x00;
        sbuf[5] = 0x00ff & (sbuf[1]+sbuf[2]+sbuf[3]+sbuf[4]);
        sbuf[6] = 0xFE;
        write(fd, sbuf, 7);
        logi("==== function %s ====\n", g_ai_enable?"enable":"disable");
        ai_switch(g_ai_enable);
    }
}

static int serial_task(void *arg)
{
#define SERIAL_DEV  "/dev/ttyS4"
#define SERIAL_BAUD_RATE    B115200
    int max_fd, ret=0;
    fd_set rfds;
    struct timeval tv;
    int serial_fd = -1;
    char rbuf[256]={0};

    prctl(PR_SET_NAME, serial_task_state.name);
    logi("---- proc %s start ----\n", serial_task_state.name);

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
                        logd("ret=%d, recv: rbuf[%d]=0x%x\n", ret, i, 0xff & rbuf[i]);
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

        delay_us(100);
    }

__exit:
    serial_exit(serial_fd);
    logi("---- proc %s stop ----\n", serial_task_state.name);
    return 0;
}

static int capture_pcm_init(snd_pcm_t **pcm)
{ 
    int ret = 0;
    alsa_api_para_t para;

    memset(&para, 0, sizeof(para));
    para.block = SND_PCM_NONBLOCK;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.stream = SND_PCM_STREAM_CAPTURE;
#if EN_32BIT
    para.format = SND_PCM_FORMAT_S32_LE;
#else
    para.format = SND_PCM_FORMAT_S16_LE;
#endif
    para.chn = CAPTURE_CHN;
    para.rate = SAMPLE_RATE;
    para.period_size = PERIOD_SIZE;
    para.buffer_size = PERIODS*PERIOD_SIZE;
    sprintf(para.card_name, "%s", CAPTURE_CARD_NAME);
    if((ret = init_pcm(pcm, para)) < 0){
        loge("init capture pcm error\n");
    }
    return ret;
}

int timeout_flag = 0;
int timeout_playnum = 0;
int first_capture = 0;
int first_ago_out = 0;
static int capture_task(void *arg)
{
    int err;
    snd_pcm_t *ppcm_capture = NULL;
    snd_pcm_state_t pcm_state;
    int cnt = 0;
    int start_err = 0;
    audio_fmt_t capture_buff[PERIOD_SIZE][CAPTURE_CHN] = {0};
    //snd_pcm_sframes_t avail_frames = 0;
    int per_sample_time = (int)(1000000.0f / SAMPLE_RATE + 0.5f);
    //int bytess_buff = sizeof(capture_buff);
    int bytess_act = sizeof(audio_fmt_t) * CAPTURE_CHN;
    snd_pcm_status_t *status;
    snd_pcm_uframes_t status_avail, status_avail_max;

    prctl(PR_SET_NAME, capture_task_state.name);
    logi("---- proc %s start ----\n", capture_task_state.name);

    snd_pcm_status_alloca(&status);

    err = capture_pcm_init(&ppcm_capture);
    if(err < 0){
        logi("capture pcm init failed\n");
        goto err_to_exit;
    }

    start_err = snd_pcm_start(ppcm_capture);
    if(start_err < 0){
        loge("first snd_pcm_start failed\n");
    }
	
    while(cnt < 20){
        pcm_state = snd_pcm_state(ppcm_capture);
        if(pcm_state == SND_PCM_STATE_RUNNING){
            logi("pcm state: %s\n", snd_pcm_state_name(pcm_state));
            break;
        }

        if(start_err < 0){
            start_err = snd_pcm_start(ppcm_capture);
            if(start_err >= 0){
                logi("restry snd_pcm_start success\n");
            }
        }

        cnt++;
        delay_ms(100);
    }
	
	int num_remain = ALG_FRAMES - 8;
	for(int i = 0; i < alg_idx_max; i++)
	{
        rb_write(g_ringbuf_ai[i], capture_buff, num_remain * bytess_act);
    }

    while (capture_task_state.running)
    {
    	int sleep_num = 1 * per_sample_time;
		
        if(ppcm_capture){
            int avail_num = snd_pcm_avail_update(ppcm_capture);
            if(avail_num >= 8)
			{
                //memset(capture_buff, 0, bytess_buff);
                int num_curr = pcm_in(ppcm_capture, capture_buff, avail_num, CAPTURE_CHN, "capture");
                if(num_curr > 0)
				{
					if(first_capture < 1)
					{
						first_capture++;
						num_curr = 8;//(num_curr > 8) ? 8 : num_curr;
					}
					
                    num_remain += num_curr;
                    for (int i = 0; i < alg_idx_max; i++) {
                        /*if(rb_get_space_free(g_ringbuf_ai[i]) < bytess_buff){
                            rb_discard(g_ringbuf_ai[i], bytess_buff);
                        }*/
                        rb_write(g_ringbuf_ai[i], capture_buff, num_curr * bytess_act);
                    }
                }

				sleep_num = 6 * per_sample_time;

                if(!check_ad_start()){
                    ad_can_be_to_start();
                }
            }

			if(avail_num < 0)
			{
                logd("err capture avail_num=%ld,%s\n", avail_num, snd_strerror(avail_num));
                check_pcm_state(ppcm_capture, avail_num, "capture");
            }
        }
		
        if(num_remain >= ALG_FRAMES){
            num_remain -= ALG_FRAMES;
            for(int i = 0; i < alg_idx_max; i++){
                sem_post(&g_sem_3a[i]);
            }
        }

        err = snd_pcm_status(ppcm_capture, status);
        if(err == 0){
            pcm_state = snd_pcm_status_get_state(status);
            status_avail = snd_pcm_status_get_avail(status);
            status_avail_max = snd_pcm_status_get_avail_max(status);
            if(pcm_state != SND_PCM_STATE_RUNNING && status_avail == 0 && status_avail_max == 0){
                logd("err capture state=%s, avail=%ld, avail_max=%ld\n", 
                    snd_pcm_state_name(pcm_state), status_avail, status_avail_max);

                snd_pcm_start(ppcm_capture);
            }
        }
		
        delay_us(sleep_num);
    }
    
err_to_exit:
    if(ppcm_capture)
        snd_pcm_close(ppcm_capture);
    logi("---- proc %s stop -----\n", capture_task_state.name);

    return 0;
}

static int playback_pcm_init(snd_pcm_t **pcm)
{ 
    int ret = 0;
    alsa_api_para_t para;

    memset(&para, 0, sizeof(para));
    para.block = SND_PCM_NONBLOCK;
    para.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    para.stream = SND_PCM_STREAM_PLAYBACK;
#if EN_32BIT
    para.format = SND_PCM_FORMAT_S32_LE;
#else
    para.format = SND_PCM_FORMAT_S16_LE;
#endif
    para.chn = PLAYBACK_CHN;
    para.rate = SAMPLE_RATE;
    para.period_size = PERIOD_SIZE;
    para.buffer_size = PERIODS*PERIOD_SIZE;
    sprintf(para.card_name, "%s", PLAYBACK_CARD_NAME);
    if((ret = init_pcm(pcm, para)) < 0){
        loge("init playback pcm error\n");
    }
    return ret;
}
#if 0
static void smooth_data(audio_fmt_t *data, int len)
{ 
    for (int i = 0; i < len; i++){
        int source_idx = i % len;
        float progress = (float)i / len;
        float factor = (1.0f + cosf(progress * M_PI)) * 0.5f;
        data[i] = (audio_fmt_t)(data[source_idx] * factor);
    } 
}
#endif

static int playback_task(void *arg)
{
    int err;
    snd_pcm_t *ppcm_playback;
    audio_fmt_t playback_buff[256][PLAYBACK_CHN] = {0};
    audio_fmt_t tmpbuff[PLAYBACK_CHN][256] = {0};
    //snd_pcm_sframes_t avail_frames;
    int per_sample_time = (int)(1000000.0f / SAMPLE_RATE + 0.5f);
    int bytess_fmt = sizeof(audio_fmt_t);
    //int bytess = bytess_fmt*PERIOD_SIZE;
	
    prctl(PR_SET_NAME, playback_task_state.name);
    logi("---- proc %s start ----\n", playback_task_state.name);

    err = playback_pcm_init(&ppcm_playback);
    if(err < 0){
        loge("playback pcm init failed\n");
        goto err_to_exit;
    }
	
    /*while(1){
        if(rb_get_space_used(g_ringbuf_ao[0]) >= bytess){
            break;
        }
        //如果不是必要的，那么不要在开始时输出静音数据，可能造成有效数据过来的时候，
        //alsa驱动buffer中有静音数据仍未播放完成，增加了延迟
        // pcm_out(ppcm_playback, playback_buff, PERIOD_SIZE, "playback");
        delay_us(100);
    }*/
	
	/*while(first_capture == 0)
	{
		delay_us(100);
	}*/

#define TRY_PLAY_SAMPLE	16
	audio_fmt_t tmp_buff[TRY_PLAY_SAMPLE][PLAYBACK_CHN] = {0};
	pcm_out(ppcm_playback, tmp_buff, TRY_PLAY_SAMPLE, PLAYBACK_CHN, "playback");
    int num_procss = 16;
    int bytess_fmt_act = num_procss * bytess_fmt;
    while (playback_task_state.running)
    {
    	int sleep_num = 10 * per_sample_time;
		
        if(ppcm_playback)
		{
			int avail_num = snd_pcm_avail_update(ppcm_playback);
			
            if(first_capture > 0 && avail_num >= num_procss)
			{
				int num_l = rb_get_space_used(g_ringbuf_ao[0]);
                if(num_l >= bytess_fmt_act)
				{
                    rb_read(g_ringbuf_ao[0], tmpbuff[0], bytess_fmt_act);
                }
                else
				{
					rb_read(g_ringbuf_ao[0], tmpbuff[0], num_l);
                    rb_read_try(g_ringbuf_fill_data[0], tmpbuff[0]+num_l, bytess_fmt_act-num_l);
					
					timeout_flag = 1;
					timeout_playnum += num_procss;
                }

				int num_r = rb_get_space_used(g_ringbuf_ao[1]);
                if(num_r >= bytess_fmt_act)
				{
                    rb_read(g_ringbuf_ao[1], tmpbuff[1], bytess_fmt_act);
                }
                else
				{
					rb_read(g_ringbuf_ao[1], tmpbuff[1], num_r);
                    rb_read_try(g_ringbuf_fill_data[1], tmpbuff[1]+num_r, bytess_fmt_act-num_r);
                }
                
                for(int i = 0; i < PLAYBACK_CHN; i++)
				{
                    for(int j = 0; j < num_procss; j++)
					{
                        playback_buff[j][i] = tmpbuff[i][j];
                    }
                }
                pcm_out(ppcm_playback, playback_buff, num_procss, PLAYBACK_CHN, "playback");
				
				sleep_num = 4 * per_sample_time;

                if(!check_ad_start()){
                    ad_can_be_to_start();
                }
            }
			
			if(avail_num < 0)
			{
                logd("err playback avail_num=%ld,%s\n", avail_num, snd_strerror(avail_num));
                check_pcm_state(ppcm_playback, avail_num, "playback");
            }
        }
		
        delay_us(sleep_num);
    }
    
err_to_exit:
    if(ppcm_playback)
        snd_pcm_close(ppcm_playback);
    logi("---- proc %s stop ----\n", playback_task_state.name);
    return 0;
}


static int alg_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0, last_cost_max = 0;
    audio_fmt_t rec_buff[ALG_FRAMES][REC_CHN] = {0};
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN] = {0};
    audio_fmt_t mic_data1[ALG_FRAMES] = {0};
    audio_fmt_t mic_data2[ALG_FRAMES] = {0};
	audio_fmt_t out_data1[ALG_FRAMES] = {0};
    audio_fmt_t out_data2[ALG_FRAMES] = {0};
    int bytess = sizeof(audio_fmt_t);
    int bytess_alg_frames = bytess*ALG_FRAMES;
    int bytess_capture_buff = bytess_alg_frames*CAPTURE_CHN;
    int bytess_rec_buff = bytess_alg_frames*REC_CHN;

    prctl(PR_SET_NAME, alg_task_state.name);
    logi("---- proc %s start ----\n", alg_task_state.name);

    gettimeofday(&tvlast, NULL);
    while(alg_task_state.running){
        sem_wait(&g_sem_3a[alg_idx_1]);

        if(rb_get_space_used(g_ringbuf_ai[alg_idx_1]) < bytess_capture_buff){
            delay_us(100);
            continue;
        }

        rb_read(g_ringbuf_ai[alg_idx_1], capture_buff, bytess_capture_buff);
        for(int i = 0; i < ALG_FRAMES; i++){
            mic_data1[i] = capture_buff[i][0];
            mic_data2[i] = capture_buff[i][1];
        }

        //memset(out_data1, 0, bytess_alg_frames);
        //memset(out_data2, 0, bytess_alg_frames);
        gettimeofday(&tvbef, NULL);
#if ENABLE_ALGO
        _algo_process1(mic_data1, mic_data2, out_data1, out_data2, NULL, NULL);
		
#else
        memcpy(out_data1, mic_data1, bytess_alg_frames);
        memcpy(out_data2, mic_data2, bytess_alg_frames);
#endif
        gettimeofday(&tvaft, NULL);

		int num_write = bytess_alg_frames;
		if(first_ago_out == 0)
		{
			num_write = 0; //bytess*(ALG_FRAMES - 16);
		}
		
		if(timeout_flag == 1)
		{
			//num_write = bytess*(ALG_FRAMES - 8);
			//num_write = bytess*(ALG_FRAMES - timeout_playnum);
			//num_write = (num_write > 0) ? num_write : 0;
			timeout_flag = 0;
			timeout_playnum = 0;
		}
		
        //if(rb_get_space_free(g_ringbuf_ao[0]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_ao[0], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_ao[0], out_data1, num_write);
        //if(rb_get_space_free(g_ringbuf_fill_data[0]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_fill_data[0], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_fill_data[0], out_data1, bytess_alg_frames);
#if TWO_OUT_DATA
        //if(rb_get_space_free(g_ringbuf_ao[1]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_ao[1], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_ao[1], out_data2, num_write);
        //if(rb_get_space_free(g_ringbuf_fill_data[1]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_fill_data[1], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_fill_data[1], out_data2, bytess_alg_frames);
#else
        //if(rb_get_space_free(g_ringbuf_ao[1]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_ao[1], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_ao[1], out_data1, num_write);
        //if(rb_get_space_free(g_ringbuf_fill_data[1]) < bytess_alg_frames){
        //    rb_discard(g_ringbuf_fill_data[1], bytess_alg_frames);
        //}
        rb_write(g_ringbuf_fill_data[1], out_data1, bytess_alg_frames);
#endif

		if(first_ago_out < 10)
		{
			first_ago_out++;
		}
		
        cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALG_COST_TIME){
            time_out_cnt++;
            if(cost_max > last_cost_max){
                double delay = cost_max - ALG_COST_TIME;
                int delay_sample = (int)round(delay * SAMPLE_PER_MS_FLOAT);//四舍五入
                int bytess_delay = bytess * delay_sample;
                last_cost_max = cost_max;
                //if(rb_get_space_free(g_ringbuf_ao[0]) < bytess_delay){
                //    rb_discard(g_ringbuf_ao[0], bytess_delay);
                //}
                rb_write(g_ringbuf_ao[0], out_data1, bytess_delay);
                #if TWO_OUT_DATA
                //if(rb_get_space_free(g_ringbuf_ao[1]) < bytess_delay){
                //    rb_discard(g_ringbuf_ao[1], bytess_delay);
                //}
                rb_write(g_ringbuf_ao[1], out_data2, bytess_delay);
                #else
                //if(rb_get_space_free(g_ringbuf_ao[1]) < bytess_delay){
                //    rb_discard(g_ringbuf_ao[1], bytess_delay);
                //}
                rb_write(g_ringbuf_ao[1], out_data1, bytess_delay);
                #endif
            }
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
            for(int i = 0; i < ALG_FRAMES; i++){
                rec_buff[i][0] = capture_buff[i][0];
                rec_buff[i][1] = capture_buff[i][1];
                rec_buff[i][2] = out_data1[i];
                #if TWO_OUT_DATA
                rec_buff[i][3] = out_data2[i];
                #else
                rec_buff[i][3] = out_data1[i];
                #endif
            }
            rb_write(g_ringbuf_rec, rec_buff, bytess_rec_buff);
        }

        usleep(100);
    }

    logi("---- proc %s stop ----\n", alg_task_state.name);
    return 0;
}

static int alg2_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0;
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN] = {0};
    audio_fmt_t mic_data1[ALG_FRAMES] = {0};
    audio_fmt_t mic_data2[ALG_FRAMES] = {0};
    int bytess = sizeof(audio_fmt_t);
    int bytess_alg_frames = bytess*ALG_FRAMES;
    int bytess_capture_buff = bytess_alg_frames*CAPTURE_CHN;

    prctl(PR_SET_NAME, alg2_task_state.name);
    logi("---- proc %s start ----\n", alg2_task_state.name);

	gettimeofday(&tvlast, NULL);
    while(alg2_task_state.running){
        sem_wait(&g_sem_3a[alg_idx_2]);

        if(rb_get_space_used(g_ringbuf_ai[alg_idx_2]) < bytess_capture_buff){
            delay_us(100);
            continue;
        }

        rb_read(g_ringbuf_ai[alg_idx_2], capture_buff, bytess_capture_buff);
        for(int i = 0; i < ALG_FRAMES; i++){
            mic_data1[i] = capture_buff[i][0];
            mic_data2[i] = capture_buff[i][1];
        }
        gettimeofday(&tvbef, NULL);
#if ENABLE_ALGO
        _algo_process2(mic_data1, mic_data2, NULL, NULL);
#endif
        gettimeofday(&tvaft, NULL);
		cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALG_COST_TIME){
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

    logi("---- proc %s stop ----\n", alg2_task_state.name);
    return 0;
}

static int alg3_task(void *arg)
{
    struct timeval tvbef, tvaft, tvcur, tvlast;
    unsigned long time_out_cnt = 0;
	double cost = 0, cost_max = 0;   
    audio_fmt_t capture_buff[ALG_FRAMES][CAPTURE_CHN] = {0};
    audio_fmt_t mic_data1[ALG_FRAMES] = {0};
    audio_fmt_t mic_data2[ALG_FRAMES] = {0};
    int bytess = sizeof(audio_fmt_t);
    int bytess_alg_frames = bytess*ALG_FRAMES;
    int bytess_capture_buff = bytess_alg_frames*CAPTURE_CHN;

    prctl(PR_SET_NAME, alg3_task_state.name);
    logi("---- proc %s start ----\n", alg3_task_state.name);

	gettimeofday(&tvlast, NULL);
    while(alg2_task_state.running){
        sem_wait(&g_sem_3a[alg_idx_3]);

        if(rb_get_space_used(g_ringbuf_ai[alg_idx_3]) < bytess_capture_buff){
            delay_us(100);
            continue;
        }

        rb_read(g_ringbuf_ai[alg_idx_3], capture_buff, bytess_capture_buff);
        for(int i = 0; i < ALG_FRAMES; i++){
            mic_data1[i] = capture_buff[i][0];
            mic_data2[i] = capture_buff[i][1];
        }

        gettimeofday(&tvbef, NULL);
#if ENABLE_ALGO
        _algo_process3(mic_data1, mic_data2, NULL, NULL);
#endif
        gettimeofday(&tvaft, NULL);
		cost = check_time_increment_ms_f(tvbef, tvaft);
        if(cost > cost_max){
            cost_max = cost;
        }
        if(cost_max > ALG_COST_TIME){
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

    logi("---- proc %s stop ----\n", alg3_task_state.name);
    return 0;
}

static void rec_start(void)
{
	char path[256] = {0};

    snprintf(path, sizeof(path), "/data/rec.wav");
    if(!p_file_rec) {
        p_file_rec = fopen(path, "w");
        wav_start_write(p_file_rec, &st_wavhead, DATA_BIT, REC_CHN, SAMPLE_RATE);
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
    audio_fmt_t wrbuff[SAMPLE_RATE][REC_CHN] = {0};
    int bytes_wrbuff = sizeof(wrbuff);
    char buf[1024] = {0};
    struct sockaddr_in start_client_addr;
    struct sockaddr_in client_addr;
    unsigned long rec_time;
    struct timeval tvcur, tvstart;

    prctl(PR_SET_NAME, rec_task_state.name);
    logi("---- proc %s start ----\n", rec_task_state.name);

    recv_udp = udp_server_init(UDP_SERVER_PORT);
    if(!recv_udp){
        loge("record recv udp init error\n");
        exit(-1);
    }
    poll_ms = 1000;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    while(rec_task_state.running){
        ret = udp_server_recv(recv_udp, &client_addr, buf, sizeof(buf), poll_ms);
        if(ret > 0){
            buf[sizeof(buf) - 1] = '\0';
            logd("recv cmmd: %s\n", buf);
            if (strstr(buf, RECORD_CMD_START)) {
                gettimeofday(&tvstart, NULL);
                rec_start();
                rb_cleanup(g_ringbuf_rec);
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
            if(rb_get_space_used(g_ringbuf_rec) >= bytes_wrbuff){
                rb_read(g_ringbuf_rec, wrbuff, bytes_wrbuff);
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

    udp_server_exit(recv_udp);

    logi("---- proc %s stop ----\n", rec_task_state.name);
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
    int size;
#define SAMPLE_CNT	4
    size = sizeof(audio_fmt_t) * CAPTURE_CHN * (ALG_FRAMES + 8 * SAMPLE_CNT);
    for(int i = 0; i < alg_idx_max; i++){
        g_ringbuf_ai[i] = rb_create(size);
    }
	
    size = sizeof(audio_fmt_t) * (ALG_FRAMES + 16 * SAMPLE_CNT);
    for(int i = 0; i < PLAYBACK_CHN; i++){
        g_ringbuf_ao[i] = rb_create(size);
    }
    size = sizeof(audio_fmt_t) * ALG_FRAMES * 2;
    for(int i = 0; i < PLAYBACK_CHN; i++){
        g_ringbuf_fill_data[i] = rb_create(size);
    }
    size = sizeof(audio_fmt_t) * SAMPLE_RATE * 2 * REC_CHN;
    g_ringbuf_rec = rb_create(size);

    for(int i = 0; i < alg_idx_max; i++){
        sem_init(&g_sem_3a[i], 0, 0);
    }

    algo_eq_init();

    ret = task_init(CPU_1, 70, "algo1", &alg_task_state, alg_task);
    if(ret < 0){
        goto err_algo1;
    }
    ret = task_init(CPU_2, 70, "algo2", &alg2_task_state, alg2_task);
    if(ret < 0){
        goto err_algo2;
    }
    ret = task_init(CPU_3, 70, "algo3", &alg3_task_state, alg3_task);
    if(ret < 0){
        goto err_algo3;
    }
    ret = task_init(CPU_0, 70, "capture", &capture_task_state, capture_task);
    if(ret < 0){
        goto err_capture;
    }

    ret = task_init(CPU_0, 70, "playback", &playback_task_state, playback_task);
    if(ret < 0){
        goto err_playback;
    }

    ret = task_init(CPU_0, 60, "serial", &serial_task_state, serial_task);
    if(ret < 0){
        goto err_serial;
    }

    ret = task_init(CPU_0, 60, "key", &key_task_state, key_task);
    if(ret < 0){
        goto err_key;
    }

    ret = task_init(CPU_0, 50, "record", &rec_task_state, rec_task);
    if(ret < 0){
        goto err_rec;
    }

    return ret;

err_rec:
    task_destroy(&key_task_state);
err_key:
    task_destroy(&serial_task_state);
err_serial:
    task_destroy(&playback_task_state);
err_playback:
    task_destroy(&capture_task_state);
err_capture:
    task_destroy(&alg3_task_state);
err_algo3:
    task_destroy(&alg2_task_state);
err_algo2:
    task_destroy(&alg_task_state);
err_algo1:
    for(int i = 0; i < alg_idx_max; i++){
        sem_destroy(&g_sem_3a[i]);
    }

    for(int i = 0; i < alg_idx_max; i++){
        rb_destroy(g_ringbuf_ai[i]);
    }
    for(int i = 0; i < PLAYBACK_CHN; i++){
        rb_destroy(g_ringbuf_ao[i]);
        rb_destroy(g_ringbuf_fill_data[i]);
    }
    rb_destroy(g_ringbuf_rec);
    return ret;
}

void audio_stop()
{
    task_destroy(&alg_task_state);
    task_destroy(&alg2_task_state);
    task_destroy(&alg3_task_state);
    task_destroy(&capture_task_state);
    task_destroy(&playback_task_state);
    task_destroy(&serial_task_state);
    task_destroy(&rec_task_state);

    for(int i = 0; i < alg_idx_max; i++){
        sem_destroy(&g_sem_3a[i]);
    }

    for(int i = 0; i < alg_idx_max; i++){
        rb_destroy(g_ringbuf_ai[i]);
    }
    for(int i = 0; i < PLAYBACK_CHN; i++){
        rb_destroy(g_ringbuf_ao[i]);
        rb_destroy(g_ringbuf_fill_data[i]);
    }
    rb_destroy(g_ringbuf_rec);
}