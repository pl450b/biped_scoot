#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/uart.h"

#include "math.h"
#include "stdlib.h"

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

#define FRONT_LEFT_SERVO             32
#define BACK_LEFT_SERVO              33 
#define BACK_RIGHT_SERVO             26
#define FRONT_RIGHT_SERVO            27

static const char *TAG = "Robot";

#define UPPER_LEG_LEN           40
#define LOWER_LEG_LEN           24
#define REAR_OFFSET             21
#define PI                      3.14159

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

int state = 0;
int left_x, left_y, right_x, right_y;
leg_t left_leg, right_leg;
static const int RX_BUF_SIZE = 1024;

static void update_legs(void) {
    set_leg_pos(&left_leg, left_x, left_y);
    set_leg_pos(&right_leg, right_x, right_y);
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

    int servo_sel = 0;
    int servo_angle = 0;
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            if(state == 0) {
                state = 1;
                servo_sel = atoi((const char*)data);
                ESP_LOGI(RX_TASK_TAG, "Servo %i selected", servo_sel);       
            }
            else if(state == 1) {
                state = 0;
                servo_angle = atoi((const char*)data);
                ESP_LOGI(RX_TASK_TAG, "Servo %i set to angle %i", servo_sel, servo_angle);
                switch(servo_sel) {
                    case 1: set_servo_angle(&left_leg.front_servo, servo_angle); break;
                    case 2: set_servo_angle(&left_leg.rear_servo, servo_angle); break;
                    case 3: set_servo_angle(&right_leg.front_servo, servo_angle); break;
                    case 4: set_servo_angle(&right_leg.rear_servo, servo_angle); break;
                    default: ESP_LOGE(RX_TASK_TAG, "Bad Servo selection"); break; 
                }
            }
            
        }
    }
    free(data);
}

void app_main()
{
    init_uart();

    esp_err_t ret = init_legs(&left_leg, &right_leg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init legs: %s", esp_err_to_name(ret));
        return;
    }

    right_x = REAR_OFFSET/2;
    right_y = 20;
    left_x = REAR_OFFSET/2;
    left_y = 20;

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, 10, NULL);
    // xTaskCreate(uart_task, "uart_task", 2048, NULL, 10, NULL);
    ESP_LOGI("SYSTEM", "uart receive task started");


}
