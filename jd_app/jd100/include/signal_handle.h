#ifndef SIGNAL_HANDLE_H_
#define SIGNAL_HANDLE_H_

#ifdef  __cplusplus
extern "C" {
#endif

void signal_hanler_init(const char *app_name, int *running);
void signal_hanler_exit(void);
int signal_hanler_sig_by_user(void);

#ifdef  __cplusplus
}
#endif

#endif /* SIGNAL_HANDLE_H_ */