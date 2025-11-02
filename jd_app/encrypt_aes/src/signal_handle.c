#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/sockios.h>
#include <linux/input.h>
#include <math.h>
#include <ctype.h>
#include "thread.h"
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

static char *global_app_name;
static int *thread_running = NULL;
static bool is_exit_by_user = false;

static const char *signal_str[] = {
[1]="SIGHUP",       [2]="SIGINT",       [3]="SIGQUIT",      [4]="SIGILL",       [5]="SIGTRAP",
[6]="SIGABRT",      [7]="SIGBUS",       [8]="SIGFPE",       [9]="SIGKILL",      [10]="SIGUSR1",
[11]="SIGSEGV",     [12]="SIGUSR2",     [13]="SIGPIPE",     [14]="SIGALRM",     [15]="SIGTERM",
[16]="SIGSTKFLT",   [17]="SIGCHLD",     [18]="SIGCONT",     [19]="SIGSTOP",     [20]="SIGTSTP",
[21]="SIGTTIN",     [22]="SIGTTOU",     [23]="SIGURG",      [24]="SIGXCPU",     [25]="SIGXFSZ",
[26]="SIGVTALRM",   [27]="SIGPROF",     [28]="SIGWINCH",    [29]="SIGIO",       [30]="SIGPWR",
[31]="SIGSYS",      [34]="SIGRTMIN",    [35]="SIGRTMIN+1",  [36]="SIGRTMIN+2",  [37]="SIGRTMIN+3",
[38]="SIGRTMIN+4",  [39]="SIGRTMIN+5",  [40]="SIGRTMIN+6",  [41]="SIGRTMIN+7",  [42]="SIGRTMIN+8",
[43]="SIGRTMIN+9",  [44]="SIGRTMIN+10", [45]="SIGRTMIN+11", [46]="SIGRTMIN+12", [47]="SIGRTMIN+13",
[48]="SIGRTMIN+14", [49]="SIGRTMIN+15", [50]="SIGRTMAX-14", [51]="SIGRTMAX-13", [52]="SIGRTMAX-12",
[53]="SIGRTMAX-11", [54]="SIGRTMAX-10", [55]="SIGRTMAX-9",  [56]="SIGRTMAX-8",  [57]="SIGRTMAX-7",
[58]="SIGRTMAX-6",  [59]="SIGRTMAX-5",  [60]="SIGRTMAX-4",  [61]="SIGRTMAX-3",  [62]="SIGRTMAX-2",
[63]="SIGRTMAX-1",  [64]="SIGRTMAX"
};

static void sig_handler(int signo)
{
    void *array[20];
    int size = 0;
    char **strings = NULL;
    int i = 0;

    DEBUG_PRINT("\n\n%s(%d) catch by signal %d\n",
                global_app_name, getpid(), signo);

    if(signo < sizeof(signal_str)/sizeof(signal_str[0]) && signal_str[signo]) {
        DEBUG_PRINT("\n\n%s(%d) catch by signal %s.\n",
                global_app_name, getpid(), signal_str[signo]);
    }
    
    size = backtrace(array, 20);
    strings = backtrace_symbols(array, size);
    DEBUG_PRINT("Call Trace: size=%d\n", size);

    if (strings) {
        for (i = 0; i < size; i++)
            DEBUG_PRINT("  %s\n", strings[i]);
        free(strings);
    } else {
        DEBUG_PRINT("Not Found\n\n");
    }

    if(thread_running) {
        *thread_running = false;
    }

    if(signo == SIGUSR1 || signo == SIGINT) {
        is_exit_by_user = true;
    }
}

void signal_hanler_init(const char *app_name, int *running)
{
    static struct sigaction act;

    global_app_name = strdup(app_name);
    thread_running = running;

    act.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&act.sa_mask);
    act.sa_handler = sig_handler;
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGTRAP, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGSYS, &act, NULL);
    sigaction(SIGXCPU, &act, NULL);
    sigaction(SIGXFSZ, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    
    sigaction(SIGTERM, &act, NULL);
    // sigaction(SIGKILL, &act, NULL); //Note that the two signals SIGKILL and SIGSTOP can't be caught.

    
    // SIGUSR1, 执行kill -10 audio-pro，或者killall -10 audio-pro，用于关闭看门狗
    sigaction(SIGUSR1, &act, NULL); 

    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
}

void signal_hanler_exit(void)
{
    if(global_app_name) {
        free(global_app_name);
        global_app_name = NULL;
    }
}

bool signal_hanler_sig_by_user(void)
{
    return is_exit_by_user;
}

#ifdef __cplusplus
}
#endif