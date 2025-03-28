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
#include "mpu6050_dev.h"

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
char msg_buffer[100];

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    rxQueue = xQueueCreate(100, sizeof(char[100]));
    txQueue = xQueueCreate(100, sizeof(char[100]));
    
    // init_I2C();
    // ESP_LOGI("SYSTEM", "Init i2c with MPU6050 complete");
    wifi_init_sta();
    // ESP_LOGI("SYSTEM", "Init wifi complete");
    init_leg(&left_leg, FRONT_LEFT_SERVO, BACK_LEFT_SERVO, 0);
    init_leg(&right_leg, FRONT_RIGHT_SERVO, BACK_RIGHT_SERVO, 1);
    ESP_LOGI("SYSTEM", "Init legs complete");
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);

    while(1) {
        if(xQueueReceive(rxQueue, &msg_buffer, (TickType_t)0) != pdPASS) {
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            int index = 0;
            int servo_angles[4];
            std::stringstream ss(msg_buffer);
            std::string token;
            while (std::getline(ss, token, ',') && index < 4) {
                servo_angles[index++] = std::stoi(token);  // Convert to int
            }
            ESP_LOGI("SYSTEM", "values: %i,%i,%i,%i", servo_angles[0], servo_angles[1], servo_angles[2], servo_angles[3]);
            set_servo_angle(&left_leg, true, servo_angles[0]);
            set_servo_angle(&left_leg, false, servo_angles[1]);
            set_servo_angle(&right_leg, true, servo_angles[2]);
            set_servo_angle(&right_leg, false, servo_angles[3]);

            memset(msg_buffer, 0, sizeof(msg_buffer));
        }
    }


    // int ag1 = 33;
    // int ag2 = 77;
    // while(1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     set_servo_angle(&right_leg, true, ag1);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     set_servo_angle(&right_leg, true, ag2);
    // }
}
