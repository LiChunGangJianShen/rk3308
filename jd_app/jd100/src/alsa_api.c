#include "log.h"
#include "alsa_api.h"

static int set_pcm_params(snd_pcm_t *ppcm, alsa_api_para_t alsa_params)
{
    int err = 0;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;

    snd_pcm_hw_params_alloca(&hw_params);
    if((err = snd_pcm_hw_params_any(ppcm, hw_params)) < 0){
        log_dbg("snd_pcm_hw_params_any error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_access(ppcm, hw_params, alsa_params.access)) < 0){
        log_dbg("snd_pcm_hw_params_set_access error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_format(ppcm, hw_params, alsa_params.format)) < 0){
        log_dbg("snd_pcm_hw_params_set_format error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_channels(ppcm, hw_params, alsa_params.chn)) < 0){
        log_dbg("snd_pcm_hw_params_set_channels error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_rate(ppcm, hw_params, alsa_params.rate, 0)) < 0){
        log_dbg("snd_pcm_hw_params_set_rate error(%s)", snd_strerror(err));
        return err;
    }

    snd_pcm_uframes_t period_size = alsa_params.period_size;
    if((err = snd_pcm_hw_params_set_period_size_near(ppcm, hw_params, &period_size, 0)) < 0){
        log_dbg("snd_pcm_hw_params_set_period_size error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params_set_buffer_size(ppcm, hw_params, alsa_params.buffer_size)) < 0){
        log_dbg("snd_pcm_hw_params_set_buffer_size error(%s)", snd_strerror(err));
        return err;
    }

    if((err = snd_pcm_hw_params(ppcm, hw_params)) < 0){
        log_dbg("snd_pcm_hw_params error(%s)", snd_strerror(err));
        return err;
    }

    snd_pcm_sw_params_alloca(&sw_params);
    if((err = snd_pcm_sw_params_current(ppcm, sw_params)) < 0){
        log_dbg("snd_pcm_sw_params_current error(%s)", snd_strerror(err));
        return err;
    }

    if(alsa_params.stream == SND_PCM_STREAM_CAPTURE){
        if((err = snd_pcm_sw_params_set_start_threshold(ppcm, sw_params, 0)) < 0){
            log_dbg("snd_pcm_sw_params_set_start_threshold error(%s)", snd_strerror(err));
            return err;
        }
        if((err = snd_pcm_sw_params_set_stop_threshold(ppcm, sw_params, alsa_params.buffer_size)) < 0){
            log_dbg("snd_pcm_sw_params_set_stop_threshold error(%s)", snd_strerror(err));
            return err;
        }
    }
    else if(alsa_params.stream == SND_PCM_STREAM_PLAYBACK){
        if((err = snd_pcm_sw_params_set_start_threshold(ppcm, sw_params, alsa_params.period_size)) < 0){
            log_dbg("snd_pcm_sw_params_set_start_threshold error(%s)", snd_strerror(err));
            return err;
        }
        if((err = snd_pcm_sw_params_set_stop_threshold(ppcm, sw_params, alsa_params.buffer_size)) < 0){
            log_dbg("snd_pcm_sw_params_set_stop_threshold error(%s)", snd_strerror(err));
            return err;
        }
    }

    if((err = snd_pcm_sw_params(ppcm, sw_params)) < 0){
        log_dbg("snd_pcm_sw_params error(%s)", snd_strerror(err));
        return err;
    }

    return err;
}

int init_pcm(snd_pcm_t **ppcm, alsa_api_para_t alsa_params)
{
    int err = 0;

    log_dbg("card name(%s)", alsa_params.card_name);
    if((err = snd_pcm_open(ppcm, alsa_params.card_name, alsa_params.stream, alsa_params.block)) < 0){
        log_dbg("snd_pcm_open error(%s)", snd_strerror(err));
        return -1;
    }

    if((err = set_pcm_params(*ppcm, alsa_params)) < 0){
        log_dbg("set_pcm_params error");
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
            log_dbg("Can't recovery from underrun, prepare failed: %s", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(ppcm)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(ppcm);
            if (err < 0)
                log_dbg("Can't recovery from suspend, prepare failed: %s", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

int pcm_in(snd_pcm_t *ppcm, void *buf, int size, const char *card_name)
{
    int err = 0;

    err = snd_pcm_readi(ppcm, buf, size);
    if(err < 0){
        log_dbg("pcm(%s) in xrun(%s)", card_name, snd_strerror(err));
        err = xrun_recovery(ppcm, err);
    }

    return err;
}

int pcm_out(snd_pcm_t *ppcm, void *buf, int size, const char *card_name)
{
    int err = 0;

    err = snd_pcm_writei(ppcm, buf, size);
    if(err < 0){
        log_dbg("pcm(%s) out xrun(%s)", card_name, snd_strerror(err));
        err = xrun_recovery(ppcm, err);
    }

    return err;
}


int check_pcm_state(snd_pcm_t *ppcm, int state, const char *alias)
{
    int ret = -1;
    if(!ppcm){
        log_dbg("invalid ppcm");
        return -1;
    }

    if(-EPIPE == state){
        log_dbg("[%s] xrun occurred(%s)", alias, snd_strerror(state));
        snd_pcm_prepare(ppcm);
    }
    else if(-ESTRPIPE == state){
        log_dbg("[%s] suspend occurred(%s)", alias, snd_strerror(state));
        while((ret = snd_pcm_resume(ppcm)) == -EAGAIN)
            usleep(1000);
        if(ret == -ENOSYS){
            log_dbg("use the snd_pcm_prepare to recovery(%s)", snd_strerror(ret));
            ret = snd_pcm_prepare(ppcm);
        }
    }
    else if(state < 0){
        log_dbg("%s", snd_strerror(state));
    }

    return ret;
}
