#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_ALGO 1

int capture_task_init(int cpu);
void capture_task_exit(void);

int playback_task_init(int cpu);
void playback_task_exit(void);

int alg_task_init(int cpu);
void alg_task_exit(void);

int alg2_task_init(int cpu);
void alg2_task_exit(void);

int alg3_task_init(int cpu);
void alg3_task_exit(void);

int rec_task_init(int cpu);
void rec_task_exit(void);

void _sem_init(void);
void _sem_destroy(void);

void init_rngbuff(void);
void exit_rngbuff(void);

#ifdef __cplusplus
}
#endif

#endif