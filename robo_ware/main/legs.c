#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#include "legs.h"

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

esp_err_t init_servo(servo_config_t *servo, mcpwm_timer_handle_t *timer, int gpio_num, int clock_group) {
    //servo->oper = NULL;
    servo->oper_config.group_id = clock_group; // operator must be in the same group to the timer
    servo->oper = NULL;
    ESP_LOGI(SERVO_TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_new_operator(&servo->oper_config, &servo->oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(servo->oper, *timer));

    ESP_LOGI(SERVO_TAG, "Create comparator and generator from the operator");
    servo->comparator_config.flags.update_cmp_on_tez = true;

    servo->comparator = NULL;
    ESP_LOGI(SERVO_TAG, "Initializing operator: group_id=%d, intr_priority=%d", 
                servo->oper_config.group_id, servo->oper_config.intr_priority);
    ESP_ERROR_CHECK(mcpwm_new_comparator(servo->oper, &servo->comparator_config, &servo->comparator));
    servo->generator_config.gen_gpio_num = gpio_num;

    servo->generator = NULL;
    ESP_ERROR_CHECK(mcpwm_new_generator(servo->oper, &servo->generator_config, 
                                        &servo->generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(0)));

    ESP_LOGI(SERVO_TAG, "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(servo->generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(servo->generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, servo->comparator, MCPWM_GEN_ACTION_LOW)));
    return ESP_OK;
}

esp_err_t init_leg(leg_t* leg, int gpio_pin1, int gpio_pin2, int clock_group) {
    // Setup Timer
    mcpwm_timer_config_t timer_config = {
        .group_id = clock_group,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
    };
    leg->timer = NULL;
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &leg->timer));

    esp_err_t ret = init_servo(&leg->front_servo, &leg->timer, gpio_pin1, clock_group);
    if (ret != ESP_OK) {
        ESP_LOGE(LEG_TAG, "Failed to init servo on pin %i with err %s", gpio_pin1, esp_err_to_name(ret));
    }

    ret = init_servo(&leg->rear_servo, &leg->timer, gpio_pin2, clock_group);
    if (ret != ESP_OK) {
        ESP_LOGE(LEG_TAG, "Failed to init servo on pin %i with err %s", gpio_pin2, esp_err_to_name(ret));
    }

    ESP_LOGI(LEG_TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(leg->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(leg->timer, MCPWM_TIMER_START_NO_STOP));
    return ESP_OK;
}

esp_err_t set_servo_angle(leg_t* leg, bool front_servo, int angle) {
    servo_config_t *servo;
    if(front_servo) {
        servo = &leg->front_servo;
    } else {
        servo = &leg->rear_servo;
    }
    if (angle < SERVO_MIN_DEGREE || angle > SERVO_MAX_DEGREE) {
        return ESP_ERR_INVALID_ARG;
    }
    servo->current_angle = angle;
    return mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(angle));
    return ESP_OK;
}
