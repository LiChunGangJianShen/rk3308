#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "udp_client.h"
#include "thread.h"


#define RECORD_UDP_SERVER_PORT (6000)
#define RECORD_CMD_START "record-cmd-start"
#define RECORD_CMD_STOP  "record-cmd-stop"
#define RECORD_CMD_STATUS  "record-cmd-status"

int main(int argc, char *argv[])
{
    udp_client* client = udp_client_init(RECORD_UDP_SERVER_PORT);
    char buf[1024] = {0};
    int valid = FALSE;
    
    if(argc < 2) {
        udp_client_exit(client);
        printf("input param invalid\n");
        return 0;
    }
    
    if(!strcmp(argv[1], "record-start")) {
        strncpy(buf, RECORD_CMD_START, sizeof(buf));
        if(udp_client_send(client, buf, sizeof(buf)) > 0) {
            printf("%s\n", buf);
        }
        valid = TRUE;
    } else if(!strcmp(argv[1], "record-stop")) {
        strncpy(buf, RECORD_CMD_STOP, sizeof(buf));
        if(udp_client_send(client, buf, sizeof(buf)) > 0) {
            printf("%s\n", buf);
        }
        valid = TRUE;
    } else if(!strcmp(argv[1], "record-status")) {
        strncpy(buf, RECORD_CMD_STATUS, sizeof(buf));
        if(udp_client_send(client, buf, sizeof(buf)) > 0) {
            printf("%s\n", buf);
        }
        valid = TRUE;
    }

    if(valid) {
        memset(buf, 0, sizeof(buf));
        if(udp_client_recv(client, buf, sizeof(buf)) > 0) {
            printf("%s\n", buf);
        }
    } else {
        if(!strcmp(argv[1], "record-action")) {
            printf("start record...\n");
            strncpy(buf, RECORD_CMD_START, sizeof(buf));
            if(udp_client_send(client, buf, sizeof(buf)) > 0) {
                if(udp_client_recv(client, buf, sizeof(buf)) > 0) {
                    printf("%s\n", buf);
                }
            }
            if(argv[2]){
                memset(buf, 0, sizeof(buf));
                strncpy(buf, argv[2], sizeof(buf));
                if(udp_client_send(client, buf, sizeof(buf)) > 0) {
                    printf("%s\n", buf);
                }
            }
            while (1) {
                strncpy(buf, RECORD_CMD_STATUS, sizeof(buf));
                if(udp_client_send(client, buf, sizeof(buf)) > 0) {
                    if(udp_client_recv(client, buf, sizeof(buf)) > 0) {
                        printf("%s\n", buf);
                        if(strstr(buf, "=end_of_record")){
                            strncpy(buf, RECORD_CMD_STOP, sizeof(buf));
                            if(udp_client_send(client, buf, sizeof(buf)) > 0) {
                                printf("%s\n", buf);
                            }
                        }
                        if(strstr(buf, "=finish")) {
                            sleep(1);
                            printf("stop record...\n");
                            break;
                        }
                    }
                }
                sleep(1);
            }
        }
    }

    udp_client_exit(client);
    return 0;
}
