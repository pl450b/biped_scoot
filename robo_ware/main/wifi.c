#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"

#include "wifi.h"

/* AP Configuration */  
#define WIFI_AP_SSID                "WesleyNetwork"
#define WIFI_AP_PASSWD              "WesleyNetwork5"
#define WIFI_CHANNEL                 6
#define MAX_STA_CONN                 18
#define PORT                         3333                    // TCP port number for the server
#define KEEPALIVE_IDLE               240
#define KEEPALIVE_INTERVAL           10
#define KEEPALIVE_COUNT              5
#define WIFI_RETRY_COUNT            10  // After 10 connect attemps, unit resets
#define ROBO_PASS                   "12345"
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

extern QueueHandle_t txQueue;
extern QueueHandle_t rxQueue;

static const char *TAG = "WIFI";
static TaskHandle_t upd_Handle = NULL;

/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { 
        if (s_retry_num < WIFI_RETRY_COUNT) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_restart();
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASSWD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to wifi network!");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to wifi network");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static const char *payload = "Crazzzyyyy Message!!!!";
static void udp_client_task(void *pvParameters)
{
    int addr_family = 0;
    int ip_protocol = 0;
    char ip_addr_str[128];

    strcpy(ip_addr_str, (char*)pvParameters);   //Copy passed IP in case another socket is opened

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(ip_addr_str);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", ip_addr_str, PORT);
        
        bool temp_alive = true;
        uint32_t notify_val;
        while (1) {
            
            for(int i = 0; i < 100; i++) {
                sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            
            if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notify_val,0) == pdTRUE) {
                temp_alive = false;
                break;
            }
        }

        ESP_LOGE(TAG, "Shutting down socket and retrying...");
        shutdown(sock, 0);
        close(sock);
        if(!temp_alive) { // if the tcp socket is off, kill this task
            vTaskDelete(NULL);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    char msg_buffer[128];

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string

        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        strcpy(msg_buffer, "Enter passwd: ");
        send(sock, msg_buffer, strlen(msg_buffer), 0);

        int received = recv(sock, msg_buffer, strlen(msg_buffer), 0);
        if(received == 0) {
            close(sock);
            continue;
        }
        msg_buffer[received-1] = '\0'; 

        // Password Check
        if(strcmp(msg_buffer, ROBO_PASS) == 0) {
            strcpy(msg_buffer, "Password corret, you may now send commands\n");
            send(sock, msg_buffer, strlen(msg_buffer), 0);
            // Start sending sensor data through UDP socket
            xTaskCreate(udp_client_task, "udp_socket", 4096, addr_str, 5, &upd_Handle);   
            // Start reading commands from socket
            connection_start(sock);         
        } else{ 
            ESP_LOGE(TAG, "Wrong password, got %s", msg_buffer);
            strcpy(msg_buffer, "Wrong password, goodbye");
            send(sock, msg_buffer, strlen(msg_buffer), 0);
            shutdown(sock, 0);
            close(sock);
        }        
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void connection_start(int tcp_socket) {
    // Send instructions    
    char tx_buffer[] = "Send servo commands in the following format:\n\
    <front left>,<rear left>,<front right>,<rear right>\n";
    
    send(tcp_socket, tx_buffer, strlen(tx_buffer), 0);                         
    
    // Read for incoming instructions
    while(1) {
        char rx_buffer[128]; 
        int received = recv(tcp_socket, rx_buffer, sizeof(rx_buffer)-1, 0);
        if (received == 0) {
            xTaskNotify(upd_Handle, (uint32_t)0, eNoAction);
            break;
        } else if (received < 0) {
            ESP_LOGE(TAG, "Socket recv error %i, exiting", received);
            close(tcp_socket);
            break;
        } else {
            BaseType_t tx_err = xQueueSend(rxQueue, &rx_buffer, (TickType_t)10);
            if(tx_err != pdPASS) {
                ESP_LOGE(TAG, "push failed with error %i", tx_err);
            }
        }
    }

}