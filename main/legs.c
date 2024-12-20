#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#include "legs.h" // IDK if I need this??

#define FRONT_LEFT_SERVO             32
#define BACK_LEFT_SERVO              33 
#define BACK_RIGHT_SERVO             26
#define FRONT_RIGHT_SERVO            27

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define UPPER_LEG_LEN           40
#define LOWER_LEG_LEN           24
#define REAR_OFFSET             21
#define PI                      3.14159

static const char *TAG = "SERVO INIT";

#define UPPER_LEG_LEN           40
#define LOWER_LEG_LEN           24
#define REAR_OFFSET             21
#define PI                      3.14159

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms


static inline uint32_t angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

void calc_angle(int x, int y, int *front_angle, int *rear_angle) {
    double a1, a2, b1, b2;     // angles
    double hypo1, hypo2;

    // Calc hypotenuse assuming 0, 0 is on the front servo axis
    hypo1 = sqrt(pow(x, 2) + pow(y, 2));
    hypo2 = sqrt(pow(x - REAR_OFFSET, 2) + pow(y, 2));

    a1 = acos(x / hypo1);
    a2 = acos((REAR_OFFSET - x)/hypo2);
    b1 = acos((pow(UPPER_LEG_LEN, 2) - pow(LOWER_LEG_LEN, 2) - pow(hypo1, 2))/(-2*LOWER_LEG_LEN*hypo1));
    b2 = acos((pow(UPPER_LEG_LEN, 2) - pow(LOWER_LEG_LEN, 2) - pow(hypo2, 2))/(-2*LOWER_LEG_LEN*hypo2));

    *front_angle = (180*(a1 + b1) / PI);      // -135 to account for servo horn setting
    *rear_angle = (180*(a2 + b2) / PI);
}

esp_err_t init_servo(servo_config_t *servo, mcpwm_timer_handle_t *timer, int gpio_num, int clock_group) {
    servo->oper = NULL;
    servo->oper_config = (mcpwm_operator_config_t) {
        .group_id = clock_group, // operator must be in the same group to the timer
    };

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_new_operator(&servo->oper_config, &servo->oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(servo->oper, *timer));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    servo->comparator_config = (mcpwm_comparator_config_t) {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(servo->oper, &servo->comparator_config, &servo->comparator));
    servo->generator_config = (mcpwm_generator_config_t) {
        .gen_gpio_num = gpio_num,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(servo->oper, &servo->generator_config, &servo->generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(0)));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(servo->generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(servo->generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, servo->comparator, MCPWM_GEN_ACTION_LOW)));
    return ESP_OK;
}

esp_err_t init_legs(leg_t* left_leg, leg_t* right_leg) {
    // Set internal leg identifies (for angle calculation)
    left_leg->left_leg = true;
    right_leg->left_leg = false;
    // Setup Timers
    left_leg->timer = NULL;
    right_leg->timer = NULL;
    mcpwm_timer_config_t timer_left_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    mcpwm_timer_config_t timer_right_config = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_left_config, &left_leg->timer));
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_right_config, &right_leg->timer));
    ESP_LOGI(TAG, "Timers made!");

    // Left Leg Setup
    esp_err_t ret = init_servo(&left_leg->front_servo, &left_leg->timer, FRONT_LEFT_SERVO, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init front left servo: %s", esp_err_to_name(ret));
        return ret;
    }
    left_leg->front_servo.angle_offset = 145;
    
    ret = init_servo(&left_leg->rear_servo, &left_leg->timer, BACK_LEFT_SERVO, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init rear left servo: %s", esp_err_to_name(ret));
        return ret;
    }
    left_leg->rear_servo.angle_offset = 138;
    ESP_LOGI(TAG, "Left leg setup!");


    // Right Leg Setup
    ret = init_servo(&right_leg->front_servo, &right_leg->timer, FRONT_RIGHT_SERVO, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init front right servo: %s", esp_err_to_name(ret));
        return ret;
    }
    right_leg->front_servo.angle_offset = 145;

    ret = init_servo(&right_leg->rear_servo, &right_leg->timer, BACK_RIGHT_SERVO, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init rear right servo: %s", esp_err_to_name(ret));
        return ret;
    }
    right_leg->rear_servo.angle_offset = 138;

    ESP_LOGI(TAG, "Right leg setup!");

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(left_leg->timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(right_leg->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(left_leg->timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(right_leg->timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI("LEG INIT", "Both leg set up and ready to roll!");

    return ESP_OK;
}

// Function to set servo angle
esp_err_t set_servo_angle(servo_config_t *servo, int angle) {
    if (angle < SERVO_MIN_DEGREE || angle > SERVO_MAX_DEGREE) {
        return ESP_ERR_INVALID_ARG;
    }
    servo->current_angle = angle;
    return mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(angle));
    return ESP_OK;
}

esp_err_t set_leg_pos(leg_t* leg, int x, int y) {
    int front_angle, rear_angle;
    calc_angle(x, y, &front_angle, &rear_angle);

    if(leg->left_leg) {
        front_angle = front_angle - leg->front_servo.angle_offset;
        rear_angle = leg->rear_servo.angle_offset - rear_angle;
    }
    else {
            front_angle = leg->front_servo.angle_offset - front_angle;
            rear_angle = rear_angle - leg->rear_servo.angle_offset;
    }

    ESP_LOGI("LEG", "Set leg angles to %i, %i", front_angle, rear_angle);

    set_servo_angle(&leg->front_servo, front_angle);
    set_servo_angle(&leg->rear_servo, rear_angle);

    return ESP_OK;
}
