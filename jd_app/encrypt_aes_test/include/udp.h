#ifndef __UDP_H
#define __UDP_H

#include <stdint.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

int udp_socket_send(int sockfd, void *message, int len, const uint16_t port, struct sockaddr_in *addr);
int udp_socket_recv(int sockfd, void *buffer, int len, struct sockaddr_in *addr, socklen_t *addr_len);
int udp_socket_init(const uint16_t port, int en_broad_cast);
void udp_socket_delete(int fd);

#ifdef __cplusplus
}
#endif

#endif