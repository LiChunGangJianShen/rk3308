#ifndef __UDP_SERVER_H_
#define __UDP_SERVER_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct 
{
int bind_port;
int sock;
}udp_server;

udp_server* udp_server_init(int bind_port);
void udp_server_exit(udp_server *server);
int udp_server_send(udp_server *server, struct sockaddr_in *client_addr, void *buf, size_t len);
int udp_server_recv(udp_server *server, struct sockaddr_in *client_addr, void *buf, size_t buf_len, int poll_ms);


#ifdef  __cplusplus
}
#endif

#endif