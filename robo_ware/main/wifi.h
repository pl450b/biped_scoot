#ifndef WIFI_H
#define WIFI_H

typedef struct {
    char* ip_addr;
    int sock;
} udp_params_t;

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_sta(void);

void tcp_server_task(void *pvParameters);

void connection_start(int tcp_socket);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
