#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "leg_controller";

// Servo configuration
#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MIN_DEGREE        -90
#define SERVO_MAX_DEGREE        90

// Pin definitions for two legs (2 servos per leg)
#define LEFT_LEG_SERVO1    32  // Hip servo
#define LEFT_LEG_SERVO2    33  // Knee servo
#define RIGHT_LEG_SERVO1   26  // Hip servo
#define RIGHT_LEG_SERVO2   27  // Knee servo

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000
#define SERVO_TIMEBASE_PERIOD        20000

// Structure to hold servo configurations
typedef struct {
    mcpwm_cmpr_handle_t comparator;
    mcpwm_gen_handle_t generator;
    int current_angle;
} servo_config_t;

// Structure to hold leg configurations
typedef struct {
    servo_config_t hip;
    servo_config_t knee;
} leg_config_t;

// Global variables for leg control
static leg_config_t left_leg;
static leg_config_t right_leg;

// Convert angle to PWM compare value
static inline uint32_t angle_to_compare(int angle) {
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / 
           (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

// Function to set servo angle
static esp_err_t set_servo_angle(servo_config_t *servo, int angle) {
    if (angle < SERVO_MIN_DEGREE || angle > SERVO_MAX_DEGREE) {
        return ESP_ERR_INVALID_ARG;
    }
    servo->current_angle = angle;
    return mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(angle));
}

// Initialize a single servo
static esp_err_t init_servo(servo_config_t *servo, mcpwm_oper_handle_t oper, 
                           mcpwm_cmpr_handle_t comparator, int gpio_num) {
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = gpio_num,
    };
    
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config, &servo->generator));
    
    // Set generator actions
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        servo->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
    ));
    
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        servo->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)
    ));
    
    servo->comparator = comparator;
    servo->current_angle = 0;
    
    return ESP_OK;
}

// Function to initialize all servos
static esp_err_t init_leg_system(void) {
    // Create timer
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // Create operators
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // Create comparator
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // Initialize all servos
    ESP_ERROR_CHECK(init_servo(&left_leg.hip, oper, comparator, LEFT_LEG_SERVO1));
    ESP_ERROR_CHECK(init_servo(&left_leg.knee, oper, comparator, LEFT_LEG_SERVO2));
    ESP_ERROR_CHECK(init_servo(&right_leg.hip, oper, comparator, RIGHT_LEG_SERVO1));
    ESP_ERROR_CHECK(init_servo(&right_leg.knee, oper, comparator, RIGHT_LEG_SERVO2));

    // Start timer
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    return ESP_OK;
}

// Function to set leg position
static esp_err_t set_leg_position(leg_config_t *leg, int hip_angle, int knee_angle) {
    ESP_ERROR_CHECK(set_servo_angle(&leg->hip, hip_angle));
    ESP_ERROR_CHECK(set_servo_angle(&leg->knee, knee_angle));
    return ESP_OK;
}

// Example walking pattern
void perform_walking_cycle(void) {
    // Move left leg forward, right leg back
    set_leg_position(&left_leg, 30, -20);  // Hip forward, knee slightly bent
    set_leg_position(&right_leg, -30, -20); // Hip back, knee slightly bent
    vTaskDelay(pdMS_TO_TICKS(500));

    // Switch positions
    set_leg_position(&left_leg, -30, -20);
    set_leg_position(&right_leg, 30, -20);
    vTaskDelay(pdMS_TO_TICKS(500));
}

void app_main(void) {
    ESP_LOGI(TAG, "Initializing leg servo system");
    ESP_ERROR_CHECK(init_leg_system());

    // Center all servos initially
    set_leg_position(&left_leg, 0, 0);
    set_leg_position(&right_leg, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting walking cycle");
    while (1) {
        perform_walking_cycle();
    }
}