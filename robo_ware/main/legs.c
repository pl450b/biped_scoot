#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#include "legs.h"
#include <string.h>

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define UPPER_LEG_LEN           40
#define LOWER_LEG_LEN           24
#define REAR_OFFSET             21
#define PI                      3.14159

static const char *SERVO_TAG = "Servo System";
static const char *LEG_TAG   = "Leg System";

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

inline uint32_t angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

esp_err_t init_servo(servo_config_t *servo, mcpwm_oper_handle_t oper, int gpio_num) {
    esp_err_t ret;

    // Establish Comparator
    memset(&servo->comparator_config, 0, sizeof(servo->comparator_config));
    servo->comparator_config.flags.update_cmp_on_tez = true;
    ret = mcpwm_new_comparator(oper, &servo->comparator_config, &servo->comparator);
    if (ret != ESP_OK) return ret;

    // Establish Generator
    memset(&servo->generator_config, 0, sizeof(servo->generator_config));
    servo->generator_config.gen_gpio_num = gpio_num;
    ret = mcpwm_new_generator(oper, &servo->generator_config, &servo->generator);
    if (ret != ESP_OK) return ret;

    // Set to center angle
    ret = mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(0));
    if (ret != ESP_OK) return ret;

    ret = mcpwm_generator_set_action_on_timer_event(
        servo->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    if (ret != ESP_OK) return ret;

    ret = mcpwm_generator_set_action_on_compare_event(
        servo->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, servo->comparator, MCPWM_GEN_ACTION_LOW));
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}


esp_err_t init_leg(leg_t* leg, int gpio_pin_a, int gpio_pin_b, int clock_group) {
    esp_err_t ret;

    mcpwm_timer_config_t timer_config = {
        .group_id = clock_group,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
    };
    ret = mcpwm_new_timer(&timer_config, &leg->timer);
    if (ret != ESP_OK) return ret;

    leg->oper_config.group_id = clock_group;
    ret = mcpwm_new_operator(&leg->oper_config, &leg->oper);
    if (ret != ESP_OK) return ret;

    ret = mcpwm_operator_connect_timer(leg->oper, leg->timer);
    if (ret != ESP_OK) return ret;

    ret = mcpwm_timer_enable(leg->timer);
    if (ret != ESP_OK) return ret;

    ret = mcpwm_timer_start_stop(leg->timer, MCPWM_TIMER_START_NO_STOP);
    if (ret != ESP_OK) return ret;

    ret = init_servo(&leg->front_servo, leg->oper, gpio_pin_a);
    if (ret != ESP_OK) return ret;

    ret = init_servo(&leg->rear_servo, leg->oper, gpio_pin_b);
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}


esp_err_t set_servo_angle(servo_config_t* servo, int angle) {
    if (servo == NULL || servo->comparator == NULL) {
        ESP_LOGE("SERVO", "Invalid servo handle or comparator is NULL!");
        return ESP_FAIL;
    }

    if (angle < SERVO_MIN_DEGREE || angle > SERVO_MAX_DEGREE) {
        ESP_LOGW("SERVO", "Angle %d out of bounds", angle);
        return ESP_ERR_INVALID_ARG;
    }

    servo->current_angle = angle;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(angle)));
    return ESP_OK;
}
