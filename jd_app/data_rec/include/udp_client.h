#ifndef __UDP_CLIENT_H_
#define __UDP_CLIENT_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>




typedef struct 
{
struct sockaddr_in server_addr;
int sock;
}udp_client;

udp_client* udp_client_init(int server_port);
void udp_client_exit(udp_client* client);
int udp_client_send(udp_client* client, void *buf, size_t len);
int udp_client_recv(udp_client* client, void *buf, size_t len);



#ifdef  __cplusplus
}
#endif

#endif