#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_ALGO 1
#define TWO_OUT_DATA    0
#define EN_REC_WAV_FILE	0

int capture_task_init(int cpu, int priority);
void capture_task_exit(void);

int playback_task_init(int cpu, int priority);
void playback_task_exit(void);

int alg_task_init(int cpu, int priority);
void alg_task_exit(void);

int alg2_task_init(int cpu, int priority);
void alg2_task_exit(void);

int alg3_task_init(int cpu, int priority);
void alg3_task_exit(void);

int rec_task_init(int cpu, int priority);
void rec_task_exit(void);

void _sem_init(void);
void _sem_destroy(void);

void init_rngbuff(void);
void exit_rngbuff(void);

int serial_task_init(int cpu, int priority);
void serial_task_exit(void);

int key_task_init(int cpu, int priority);
void key_task_exit(void);

#ifdef __cplusplus
}
#endif

#endif