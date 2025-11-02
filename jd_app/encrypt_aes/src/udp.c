#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

int udp_socket_send(int sockfd, void *message, int len, const uint16_t port, struct sockaddr_in *addr)
{
    int ret = 0;
    struct sockaddr_in target_addr;
    socklen_t addr_len = sizeof(target_addr);
    fd_set fds;
    struct timeval tval;

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    target_addr.sin_addr = addr->sin_addr;
    // inet_pton(AF_INET, addr, &target_addr.sin_addr);

    do{
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        tval.tv_sec = 0;
        tval.tv_usec = 10*1000;
    }while(select(sockfd + 1, NULL, &fds, NULL, &tval) <= 0);

    // printf("--- %s ---\n", __func__);
    ret = sendto(sockfd, message, len, 0,
              (struct sockaddr*)&target_addr, addr_len);

    return ret;
}
int udp_socket_recv(int sockfd, void *buffer, int len, struct sockaddr_in *addr, socklen_t *addr_len)
{
    int ret = 0;
    fd_set fds;
    struct timeval tval;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    tval.tv_sec = 0;
    tval.tv_usec = 10*1000;
    ret = select(sockfd + 1, &fds, NULL, NULL, &tval);
    if(ret > 0){
        ret = recvfrom(sockfd, buffer, len, 0,
            (struct sockaddr*)addr, addr_len);

        // ret = recv(sockfd, buff, len, 0);
    }
    return ret;
}

int udp_socket_init(const uint16_t port, int en_broad_cast)
{
    int sockfd = 0;
    struct sockaddr_in server_addr;

    // 1. 创建Socket并绑定到本地端口
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(en_broad_cast)
        setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &en_broad_cast, sizeof(en_broad_cast));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 绑定所有网卡
    server_addr.sin_port = htons(port);       // 绑定端口
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void udp_socket_delete(int fd)
{
    close(fd);
}

#ifdef __cplusplus
}
#endif