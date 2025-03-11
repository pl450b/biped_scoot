#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/uart.h"
#include "math.h"

#include "secrets.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "legs.h"
#include "wifi.h"
#include "mpu6050_dev.h"

// #define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
// #define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

// int left_x, left_y, right_x, right_y;
// leg_t left_leg, right_leg;

extern "C" void app_main();


QueueHandle_t txQueue;
QueueHandle_t rxQueue;

void app_main()
{
    rxQueue = xQueueCreate(1, sizeof(char[256]));
    txQueue = xQueueCreate(1, sizeof(char[256]));
    
    init_I2C();
    ESP_LOGI("SYSTEM", "Init i2c with MPU6050 complete");
    LegSystem legs;
    ESP_LOGI("SYSTEM", "Init legs complete");
    // xTaskCreate(tcp_server_task, "tcp_server", 4096, 5, NULL);
}
