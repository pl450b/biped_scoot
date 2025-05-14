#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <iostream>
#include <sstream>
#include <string>

#include "secrets.h"
#include "legs.h"
#include "wifi.h"

#define FRONT_LEFT_SERVO             32
#define BACK_LEFT_SERVO              33 
#define BACK_RIGHT_SERVO             26
#define FRONT_RIGHT_SERVO            27

extern "C" { 
    void app_main();
}

QueueHandle_t txQueue;
QueueHandle_t rxQueue;

leg_t left_leg;
leg_t right_leg;


void socket_to_queue(void *pvParameters) {
    int index = 0;
    int servo_angles[4];
    char msg_buffer[128];

    while(1) {
        xQueueReceive(rxQueue, &msg_buffer, portMAX_DELAY);
      
        index = 0;
        std::stringstream ss(msg_buffer);
        std::string token;
        while (std::getline(ss, token, ',') && index < 4) {
            servo_angles[index++] = std::stoi(token);  // safe conversion
        }

        if (index == 4) {
            ESP_LOGI("SYSTEM", "values: %i,%i,%i,%i", servo_angles[0], servo_angles[1], servo_angles[2], servo_angles[3]);
            set_servo_angle(&left_leg.front_servo, servo_angles[0]);
            set_servo_angle(&left_leg.rear_servo,  servo_angles[1]);
            set_servo_angle(&right_leg.front_servo, servo_angles[2]);
            set_servo_angle(&right_leg.rear_servo,  servo_angles[3]);
        } else {
            ESP_LOGW("SYSTEM", "Received incomplete data (%d angles)", index);
        }
        memset(msg_buffer, 0, sizeof(msg_buffer));
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    rxQueue = xQueueCreate(100, sizeof(char[128]));
    txQueue = xQueueCreate(100, sizeof(char[128]));
    
    // init_I2C();
    // ESP_LOGI("SYSTEM", "Init i2c with MPU6050 complete");
    wifi_init_sta();
    // ESP_LOGI("SYSTEM", "Init wifi complete");
    esp_err_t ret = init_leg(&left_leg, FRONT_LEFT_SERVO, BACK_LEFT_SERVO, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize left leg: %s", esp_err_to_name(ret));
        abort();
    }
    // init_leg(&right_leg, FRONT_RIGHT_SERVO, BACK_RIGHT_SERVO, 1);
    init_leg(&right_leg, BACK_RIGHT_SERVO, FRONT_RIGHT_SERVO, 1);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize right leg: %s", esp_err_to_name(ret));
        abort(); 
    }

    ESP_LOGI("SYSTEM", "Init legs complete");
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    xTaskCreate(socket_to_queue, "sock_queue", 4096, NULL, 5, NULL);   
}
