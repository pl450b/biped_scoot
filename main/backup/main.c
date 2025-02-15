#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/uart.h"
#include "math.h"

#include "legs.h" // IDK if I need this??
#include "secrets.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_SSID                   SECRET_WIFI_SSID
#define WIFI_PASS                   SECRET_WIFI_PASS
#define PORT                        SECRET_PORT                 // Port of the server
#define SERVER_IP                   "192.168.4.1"   // Default IP of ESP32 AP mode
#define EXAMPLE_ESP_MAXIMUM_RETRY   10

static const char *TAG = "Robot";

#define REAR_OFFSET             21

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

int left_x, left_y, right_x, right_y;
leg_t left_leg, right_leg;
static const int RX_BUF_SIZE = 1024;

static void update_legs(void) {
    set_leg_pos(&left_leg, left_x, left_y);
    set_leg_pos(&right_leg, left_x, left_y);
    // set_leg_pos(&right_leg, right_x, right_y);
    ESP_LOGI("LEG UPDATE", "Set left leg to (%i,%i) and right leg to (%i,%i)", left_x, left_y, right_x, right_y);
}

void init_uart(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            switch(data[0]) {
                case 'w': left_y += 5; break;
                case 'a': left_x += 5; break;
                case 's': left_y -= 5; break;
                case 'd': left_x -= 5; break;
                case 'i': right_y += 5; break;
                case 'j': right_x += 5; break;
                case 'k': right_y -= 5; break;
                case 'l': right_x -= 5; break;
                case 'r': left_x = 15;
                          left_y =  15;
                          right_x = 15;
                          right_y = 15; 
                          break;
                default: ESP_LOGI("UART TASK", "wrong data received: %c", data[0]); break; 
            }
        update_legs();
        }
    }
    free(data);
}

void app_main()
{
    init_uart();

    init_legs(&left_leg, &right_leg);
    
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, 10, NULL);
    // xTaskCreate(uart_task, "uart_task", 2048, NULL, 10, NULL);
    ESP_LOGI("SYSTEM", "uart receive task started");


}
