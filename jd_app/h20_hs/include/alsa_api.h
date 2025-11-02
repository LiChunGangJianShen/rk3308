#ifndef ALSA_API_H
#define ALSA_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <alsa/asoundlib.h>

typedef struct alsa_api_para{
    int block;
    unsigned int chn;
    unsigned int rate;
    snd_pcm_access_t access;
    snd_pcm_format_t format;
    snd_pcm_stream_t stream;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
    char card_name[64];
}alsa_api_para_t;

typedef short audio_fmt_t;
// typedef int audio_fmt_t;

int init_pcm(snd_pcm_t **ppcm, alsa_api_para_t alsa_params);
void exit_pcm(snd_pcm_t *ppcm);
int pcm_in(snd_pcm_t *ppcm, void *buf, int size, const char *card_name);
int pcm_out(snd_pcm_t *ppcm, void *buf, int size, const char *card_name);
int check_pcm_state(snd_pcm_t *ppcm, int state, const char *alias);
int alsa_cset(char *card, char *name, int value);
int get_card_num(const char *name);

#ifdef __cplusplus
}
#endif

#endif