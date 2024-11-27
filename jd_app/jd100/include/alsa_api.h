#ifndef ALSA_API_H
#define ALSA_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <alsa/asoundlib.h>
typedef struct alsa_para{
    int rate;
    int chn;
    int mode;
    snd_pcm_format_t format;
    snd_pcm_access_t access;
    snd_pcm_uframes_t period_size;
    snd_pcm_stream_t stream;
    char card_name[32];
}alsa_para_t;

int init_pcm(snd_pcm_t **pcm, alsa_para_t *para);
void destroy_pcm(snd_pcm_t *pcm);
int pcm_in(snd_pcm_t *pcm, void *buff, int size, const char *card_name);
int pcm_out(snd_pcm_t *pcm, void *buff, int size, const char *card_name);
int check_pcm_state(snd_pcm_t *pcm, int state, const char *alias);

#ifdef __cplusplus
}
#endif
#endif
