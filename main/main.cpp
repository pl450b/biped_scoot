#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/uart.h"
#include "math.h"

#include "secrets.h"
#include "mpu6050_types.h"

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

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

// int left_x, left_y, right_x, right_y;
// leg_t left_leg, right_leg;

extern void init_uart();
extern void task_initI2C();

QueueHandle_t mpuQueue;

extern "C" {
	void app_main(void);
}

void app_main()
{
    mpuQueue = xQueueCreate(1, sizeof(mpu_data_t));
    init_uart();
    ESP_LOGI("SYSTEM", "Init UART complete");
    task_initI2C();
    ESP_LOGI("SYSTEM", "Init i2c with MPU6050 complete");
}
