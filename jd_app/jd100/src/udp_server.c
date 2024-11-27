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
#include "udp_server.h"

udp_server* udp_server_init(int bind_port)
{
    struct sockaddr_in ser_addr;
    int server_fd, ret;
    udp_server* server = (udp_server*)malloc(sizeof(udp_server));

    if(!server) {
        log_err("create server malloc fail!");
        return NULL;
    }

    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        log_dbg("create socket fail!");
        free(server);
        return NULL;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    ser_addr.sin_port = htons(bind_port);

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        log_err("socket bind fail!");
        close(server_fd);
        free(server);
        return NULL;
    }

    server->bind_port = bind_port;
    server->sock = server_fd;

    return server;
}

void udp_server_exit(udp_server *server)
{
    if(server) {
        close(server->sock);
        free(server);
    }
}

int udp_server_send(udp_server *server, struct sockaddr_in *client_addr, void *buf, size_t buf_len)
{
    socklen_t len;
    
    if(server) {
        len = sizeof(struct sockaddr_in);
        return sendto(server->sock, buf, buf_len, 0, (struct sockaddr*)client_addr, len);
    } else {
        return 0;
    }
}

int udp_server_recv(udp_server *server, struct sockaddr_in *client_addr, void *buf, size_t buf_len, int poll_ms) {
    socklen_t len;
    fd_set rfds;
    struct timeval tval;
    int ret;

    FD_ZERO(&rfds);
    FD_SET(server->sock, &rfds);

    tval.tv_sec = poll_ms / 1000;
    tval.tv_usec = poll_ms % 1000 * 1000;

    if (server) {
        ret = select(server->sock + 1, &rfds, NULL, NULL, &tval);
        if (ret <= 0) {
            // log_dbg("no select");
            return 0;
        }

        if (!FD_ISSET(server->sock, &rfds)) {
            // log_dbg("sock isn't int set rfds");
            return 0;
        }

        len = sizeof(struct sockaddr_in);
        return recvfrom(server->sock, buf, buf_len, 0, (struct sockaddr *)client_addr, &len);
    } else {
        return 0;
    }
}