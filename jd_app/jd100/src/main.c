#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "log.h"
#include "jd100.h"
#include "version.h"
#include "build_time.h"
#include "rk3308_soc.h"
#include "audio.h"

static int g_rate = 22050;//def 22050
static int g_algo_periods = 256;//def 256
static int g_format = 16;//def 16

static int g_only_print_version = 0;
static int g_only_print_soctype = 0;

void help()
{
    log_info("==== help ====");
    log_info("for example\n\tjd100_app -r 32000 -f 16 -l 10");
    log_info("mean: 32k sample rate, format 16bit, 10ms algo length\n");
}

void version()
{
    char info[256];

    memset(info, 0, sizeof(info));
    sprintf(info, "\n\tversion: %s(algo: %s)\n\tbuild_time: %s", VERSION, algo_version(), BUILD_TIME);
    log_info("%s", info);
}

void argumet(int argc, char *argv[])
{
    int c = 0;
    while(1) {
        int optIndex = 0;
        static struct option longOpts[] = {
            { "rate", required_argument, NULL, 'r' },//sample rate
            { "format", required_argument, NULL, 'f' },//sample format
            { "length", required_argument, NULL, 'l' },//algo frames
            { "version", no_argument, NULL, 'v' },//version info
            { "help", no_argument, NULL, 'h' },//help
            { "soctype", no_argument, NULL, 't' },//soc-type
            { 0, 0, 0, 0 }
        };
        c = getopt_long(argc, argv, "r:f:l:vht", longOpts, &optIndex);
        if(c == -1) {
            break;
        }
        switch(c) {
            case 'r':
                g_rate = atoi(optarg);
                log_dbg("rate: %d", g_rate);
                break;
            case 'f':
                g_format = atoi(optarg);
                log_dbg("format: %d", g_format);
                break;
            case 'l':
                g_algo_periods = atoi(optarg);
                log_dbg("algo_period: %d", g_algo_periods);
                break;
            case 'v':
                version();
                g_only_print_version = 1;
                break;
            case 't':
                cpu_type();
                g_only_print_soctype = 1;
                break;
            case 'h':
            default:
                help();
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    argumet(argc, argv);
    if(g_only_print_version)
        return 0;
    if(g_only_print_soctype)
        return 0;
    
    version();
    cpu_type();
    // audio_start(g_rate, g_format, g_algo_periods);

    return 0;
}