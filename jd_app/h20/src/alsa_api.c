#include "log.h"
#include "alsa_api.h"

static int set_pcm_params(snd_pcm_t *ppcm, alsa_api_para_t alsa_params)
{
    int err = 0;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;

    snd_pcm_hw_params_alloca(&hw_params);
    if((err = snd_pcm_hw_params_any(ppcm, hw_params)) < 0){
        loge("snd_pcm_hw_params_any error(%s)\n", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_access(ppcm, hw_params, alsa_params.access)) < 0){
        loge("snd_pcm_hw_params_set_access error(%s)\n", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_format(ppcm, hw_params, alsa_params.format)) < 0){
        loge("snd_pcm_hw_params_set_format error(%s)\n", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_channels(ppcm, hw_params, alsa_params.chn)) < 0){
        loge("snd_pcm_hw_params_set_channels error(%s)\n", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_rate(ppcm, hw_params, alsa_params.rate, 0)) < 0){
        loge("snd_pcm_hw_params_set_rate error(%s)\n", snd_strerror(err));
        return err;
    }

    snd_pcm_uframes_t period_size = alsa_params.period_size;
    int dir = 0;
    if((err = snd_pcm_hw_params_set_period_size_near(ppcm, hw_params, &period_size, &dir)) < 0){
        loge("snd_pcm_hw_params_set_period_size error(%s)\n", snd_strerror(err));
        return err;
    }
    if(period_size != alsa_params.period_size){
        logi("==== period_size=%lu, alsa_params.period_size=%lu\n", period_size, alsa_params.period_size);
    }

    snd_pcm_uframes_t buffer_size = alsa_params.buffer_size;
    if((err = snd_pcm_hw_params_set_buffer_size_near(ppcm, hw_params, &buffer_size)) < 0){
        loge("snd_pcm_hw_params_set_buffer_size error(%s)\n", snd_strerror(err));
        return err;
    }
    if(buffer_size != alsa_params.buffer_size){
        logi("==== buffer_size=%lu, alsa_params.buffer_size=%lu\n", buffer_size, alsa_params.buffer_size);
    }

    if((err = snd_pcm_hw_params(ppcm, hw_params)) < 0){
        loge("snd_pcm_hw_params error(%s)\n", snd_strerror(err));
        return err;
    }

    snd_pcm_sw_params_alloca(&sw_params);
    if((err = snd_pcm_sw_params_current(ppcm, sw_params)) < 0){
        loge("snd_pcm_sw_params_current error(%s)\n", snd_strerror(err));
        return err;
    }

    if(alsa_params.stream == SND_PCM_STREAM_CAPTURE){
        int val;
        val = 0;
        if((err = snd_pcm_sw_params_set_start_threshold(ppcm, sw_params, val)) < 0){
            loge("snd_pcm_sw_params_set_start_threshold error(%s)\n", snd_strerror(err));
            return err;
        }

        val = buffer_size;
        if((err = snd_pcm_sw_params_set_stop_threshold(ppcm, sw_params, val)) < 0){
            loge("snd_pcm_sw_params_set_stop_threshold error(%s)\n", snd_strerror(err));
            return err;
        }
    }
    else if(alsa_params.stream == SND_PCM_STREAM_PLAYBACK){
        int val;
        val = period_size+1;
        if((err = snd_pcm_sw_params_set_start_threshold(ppcm, sw_params, val)) < 0){
            loge("snd_pcm_sw_params_set_start_threshold error(%s)\n", snd_strerror(err));
            return err;
        }

        val = buffer_size;
        if((err = snd_pcm_sw_params_set_stop_threshold(ppcm, sw_params, val)) < 0){
            loge("snd_pcm_sw_params_set_stop_threshold error(%s)\n", snd_strerror(err));
            return err;
        }
    }

    if((err = snd_pcm_sw_params(ppcm, sw_params)) < 0){
        loge("snd_pcm_sw_params error(%s)\n", snd_strerror(err));
        return err;
    }

    return err;
}

int init_pcm(snd_pcm_t **ppcm, alsa_api_para_t alsa_params)
{
    int err = 0;

    logd("card name(%s)\n", alsa_params.card_name);
    if((err = snd_pcm_open(ppcm, alsa_params.card_name, alsa_params.stream, alsa_params.block)) < 0){
        loge("snd_pcm_open error(%s)\n", snd_strerror(err));
        return -1;
    }

    if((err = set_pcm_params(*ppcm, alsa_params)) < 0){
        logw("set_pcm_params error\n");
        snd_pcm_close(*ppcm);
    }

    return err;
}

void exit_pcm(snd_pcm_t *ppcm)
{
    snd_pcm_drop(ppcm);
    snd_pcm_close(ppcm);
}

static int xrun_recovery(snd_pcm_t *ppcm, int err)
{
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(ppcm);
        if (err < 0)
            logw("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(ppcm)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(ppcm);
            if (err < 0)
                logw("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

int pcm_in(snd_pcm_t *ppcm, void *buf, int size, int ch, const char *card_name)
{
    int err = 0;
    int expect = size;
    int per_frame_size = ch * 2;
    int get = 0;

    while(expect){
        err = snd_pcm_readi(ppcm, buf, expect);
        if(err < 0){
            // logw("pcm(%s) in xrun(%s)\n", card_name, snd_strerror(err));
            err = xrun_recovery(ppcm, err);
        }
        else{
            expect -= err;
            get += err;
            buf = (char *)buf + err * per_frame_size;
        }
    }

    return get;
}

int pcm_out(snd_pcm_t *ppcm, void *buf, int size, int ch, const char *card_name)
{
    int err = 0;
    int expect = size;
    int per_frame_size = ch * 2;
    int put = 0;

    while(expect){
        err = snd_pcm_writei(ppcm, buf, expect);
        if(err < 0){
            // logw("pcm(%s) out xrun(%s)\n", card_name, snd_strerror(err));
            err = xrun_recovery(ppcm, err);
        }
        else{
            expect -= err;
            put += err;
            buf = (char *)buf + err * per_frame_size;
        }
    }

    return put;
}


int check_pcm_state(snd_pcm_t *ppcm, int state, const char *alias)
{
    int ret = -1;
    if(!ppcm){
        loge("invalid ppcm\n");
        return -1;
    }

    if(-EPIPE == state){
        loge("[%s] xrun occurred(%s)\n", alias, snd_strerror(state));
        snd_pcm_prepare(ppcm);
    }
    else if(-ESTRPIPE == state){
        loge("[%s] suspend occurred(%s)\n", alias, snd_strerror(state));
        while((ret = snd_pcm_resume(ppcm)) == -EAGAIN)
            usleep(1000);
        if(ret == -ENOSYS){
            loge("use the snd_pcm_prepare to recovery(%s)\n", snd_strerror(ret));
            ret = snd_pcm_prepare(ppcm);
        }
    }
    else if(state < 0){
        loge("%s\n", snd_strerror(state));
    }

    return ret;
}

int get_card_num(const char *name)
{
	if(!name){
		logw("invalid name\n");
		return -1;
	}
    //The accepted formats for "string" are:
    //The index of the card (as listed in /proc/asound/cards), given as string
    //The ID of the card (as listed in /proc/asound/cards)
    //The control device name (like /dev/snd/controlC0)
    return snd_card_get_index(name);
}

void safe_capture_pcm_close(snd_pcm_t **pcm)
{
    if(pcm == NULL || *pcm == NULL){
        return;
    }

    snd_pcm_drop(*pcm);
    snd_pcm_close(*pcm);
    *pcm = NULL;
}

void safe_playback_pcm_close(snd_pcm_t **pcm)
{
    if(pcm == NULL || *pcm == NULL){
        return;
    }

    snd_pcm_drain(*pcm);
    snd_pcm_close(*pcm);
    *pcm = NULL;
}