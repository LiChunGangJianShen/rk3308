#include "alsa_api.h"
#include "log.h"

int pcm_para_setup(snd_pcm_t *pcm, alsa_para_t *para)
{
    int rc = -1;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;

    snd_pcm_hw_params_alloca(&hw_params);

    rc = snd_pcm_hw_params_any(pcm, hw_params);
    if(rc < 0){
        log_err("snd_pcm_hw_params_any fail(%s)", snd_strerror(rc));
        return -1;
    }

    rc = snd_pcm_hw_params_set_access(pcm, hw_params, para->access);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_access fail(%s)", snd_strerror(rc));
        return -1;
    }

    rc = snd_pcm_hw_params_set_format(pcm, hw_params, para->format);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_format fail(%s)", snd_strerror(rc));
        return -1;
    }

    rc = snd_pcm_hw_params_set_rate(pcm, hw_params, para->rate, 0);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_rate fail(%s)", snd_strerror(rc));
        return -1;
    }

    log_dbg("card %s chn=%d", para->card_name, para->chn);
    rc = snd_pcm_hw_params_set_channels(pcm, hw_params, para->chn);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_channels fail(%s)", snd_strerror(rc));
        return -1;
    }

    snd_pcm_uframes_t period_size = para->period_size;
    int dir = 0;
    rc = snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period_size, &dir);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_period_size_near fail(%s)", snd_strerror(rc));
        return -1;
    }
    if(para->period_size != period_size){
        para->period_size = period_size;
    }

    snd_pcm_uframes_t buffer_size = 4*period_size;
    rc = snd_pcm_hw_params_set_buffer_size(pcm, hw_params, buffer_size);
    if(rc < 0){
        log_err("snd_pcm_hw_params_set_buffer_size_near fail(%s)", snd_strerror(rc));
        return -1;
    }

    char info[256] = {0};
    sprintf(info, "card: perido_size=%d, buffer_size=%d", (int)period_size, (int)buffer_size);
    log_info("%s", info);

    rc = snd_pcm_hw_params(pcm, hw_params);
    if(rc < 0){
        log_err("snd_pcm_hw_params fail(%s)", snd_strerror(rc));
        return -1;
    }

    snd_pcm_sw_params_alloca(&sw_params);
    rc = snd_pcm_sw_params_current(pcm, sw_params);
    if(rc < 0){
        log_err("snd_pcm_sw_params_current fail(%s)", snd_strerror(rc));
        return -1;
    }

    rc = snd_pcm_sw_params_current(pcm, sw_params);
    if(rc < 0){
        log_err("snd_pcm_sw_params_current fail(%s)", snd_strerror(rc));
        return -1;
    }

    if(para->stream == SND_PCM_STREAM_CAPTURE){
        rc = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, 0);
        if(rc < 0){
            log_err("snd_pcm_sw_params_set_start_threshold fail(%s)", snd_strerror(rc));
            return -1;
        }
        rc = snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, buffer_size);
        if(rc < 0){
            log_err("snd_pcm_sw_params_set_stop_threshold fail(%s)", snd_strerror(rc));
            return -1;
        }
    }
    else if(para->stream == SND_PCM_STREAM_PLAYBACK){
        rc = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, period_size);
        if(rc < 0){
            log_err("snd_pcm_sw_params_set_start_threshold fail(%s)", snd_strerror(rc));
            return -1;
        }
        rc = snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, buffer_size);
        if(rc < 0){
            log_err("snd_pcm_sw_params_set_stop_threshold fail(%s)", snd_strerror(rc));
            return -1;
        }
    }
    else{
        log_err("card %s invalid stream", para->card_name);
    }

    rc = snd_pcm_sw_params(pcm, sw_params);
    if(rc < 0){
        log_err("snd_pcm_sw_params fail(%s)", snd_strerror(rc));
        return -1;
    }

    return 0;
}

int init_pcm(snd_pcm_t **pcm, alsa_para_t *para)
{
    int rc = -1;

    rc = snd_pcm_open(pcm, para->card_name, para->stream, para->mode);
    if(rc < 0){
        log_err("card %s open fail", para->card_name);
        return rc;
    }

    rc = pcm_para_setup(*pcm, para);
    if(rc < 0){
        log_err("card %s aprams setup fail", para->card_name);
        return rc;
    }
    return rc;
}

void destroy_pcm(snd_pcm_t *pcm)
{
    if(pcm){
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
    }
}

static int xrun_recovery(snd_pcm_t *pcm, int err)
{
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(pcm);
        if (err < 0)
            log_dbg("Can't recovery from underrun, prepare failed: %s", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(pcm);
            if (err < 0)
                log_dbg("Can't recovery from suspend, prepare failed: %s", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

int pcm_in(snd_pcm_t *pcm, void *buff, int size, const char *card_name)
{
    int rc = 0;
    int err = 0;

    rc = snd_pcm_readi(pcm, buff, size);
    if(rc < 0){
        log_dbg("card %s overrun", card_name);
        err = rc;
        err = xrun_recovery(pcm, err);
    }

    return rc;
}

int pcm_out(snd_pcm_t *pcm, void *buff, int size, const char *card_name)
{
    int rc = 0;
    int err = 0;

    rc = snd_pcm_writei(pcm, buff, size);
    if(rc < 0){
        log_dbg("card %s underrun", card_name);
        err = rc;
        err = xrun_recovery(pcm, err);
    }

    return rc;
}

int check_pcm_state(snd_pcm_t *pcm, int state, const char *alias)
{
    int ret = -1;
    if(!pcm){
        log_dbg("invalid ppcm");
        return -1;
    }

    if(-EPIPE == state){
        log_dbg("[%s] xrun occurred(%s)", alias, snd_strerror(state));
        snd_pcm_prepare(pcm);
    }
    else if(-ESTRPIPE == state){
        log_dbg("[%s] suspend occurred(%s)", alias, snd_strerror(state));
        while((ret = snd_pcm_resume(pcm)) == -EAGAIN)
            usleep(1000);
        if(ret == -ENOSYS){
            log_dbg("use the snd_pcm_prepare to recovery(%s)", snd_strerror(ret));
            ret = snd_pcm_prepare(pcm);
        }
    }
    else if(state < 0){
        log_dbg("%s", snd_strerror(state));
    }

    return ret;
}