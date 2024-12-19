#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#include "legs.h" // IDK if I need this??
#include "secrets.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
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


void app_main()
{
    leg_t left_leg, right_leg;
    
    esp_err_t ret = init_legs(&left_leg, &right_leg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set servo angle: %s", esp_err_to_name(ret));
        return;
    }

    while(1) {
        
        for(int i = 15; i <=60; i++) {
            set_leg_pos(&left_leg, REAR_OFFSET/2, i);
            set_leg_pos(&right_leg, REAR_OFFSET/2, i);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 

        for(int i = 60; i >= 15; i--) {
            set_leg_pos(&left_leg, REAR_OFFSET/2, i);
            set_leg_pos(&right_leg, REAR_OFFSET/2, i);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 


        set_servo_angle(&left_leg.front_servo, -45);
        set_servo_angle(&right_leg.front_servo, 45);
        set_servo_angle(&left_leg.rear_servo, 45);
        set_servo_angle(&right_leg.rear_servo, -45);

        vTaskDelay(pdMS_TO_TICKS(4000)); 
    }
}
