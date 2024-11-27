#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include "log.h"
#include "udp_client.h"

udp_client* udp_client_init(int server_port)
{
    int client_fd;
    struct sockaddr_in ser_addr;
    udp_client* client = (udp_client*)malloc(sizeof(udp_client));

    if(!client) {
        loge("create client malloc fail!\n");
        return NULL;
    }

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(client_fd < 0)
    {
        loge("create socket fail!\n");
        free(client);
        return NULL;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    //ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //注意网络序转换
    ser_addr.sin_port = htons(server_port);  //注意网络序转换

    client->sock = client_fd;
    memcpy(&client->server_addr, &ser_addr, sizeof(ser_addr));
    return client;
}

void udp_client_exit(udp_client* client)
{
    if(client) {
        close(client->sock);
        free(client);
    }
}


int udp_client_send(udp_client* client, void *buf, size_t buf_len)
{
    socklen_t len;
    
    if(client) {
        len = sizeof(struct sockaddr_in);
        return sendto(client->sock, buf, buf_len, 0, (struct sockaddr*)&client->server_addr, len);
    } else {
        return 0;
    }
}

int udp_client_recv(udp_client* client, void *buf, size_t buf_len)
{
    socklen_t len;

    if(client) {
        len = sizeof(struct sockaddr_in);
        return recvfrom(client->sock, buf, buf_len, 0, (struct sockaddr*)&client->server_addr, &len);
    } else {
        return 0;
    }
}



