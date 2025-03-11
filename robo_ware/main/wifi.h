#ifndef WIFI_H
#define WIFI_H

void wifi_init_sta(void);

void tcp_server_task(void *pvParameters);

void tcp_tx_task(void *pvParameters);

void tcp_rx_task(void *pvParameters);

#endif // WIFI_H
