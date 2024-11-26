        // GPIO connects to the PWM signal line, skip 25


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "math.h"

#define FRONT_LEFT_SERVO             32
#define BACK_LEFT_SERVO              33 
#define BACK_RIGHT_SERVO             26
#define FRONT_RIGHT_SERVO            27

static const char *TAG = "Robot";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define UPPER_LEG               24
#define LOWER_LEG               40
#define REAR_OFFSET             21
#define PI                      3.14159

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static inline uint32_t angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

typedef struct {
    mcpwm_oper_handle_t oper;
    mcpwm_operator_config_t oper_config;

    mcpwm_cmpr_handle_t comparator;
    mcpwm_comparator_config_t comparator_config;

    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t generator_config;

    int current_angle;
} servo_config_t;

// Function to set servo angle
static esp_err_t set_servo_angle(servo_config_t *servo, int angle) {
    if (angle < SERVO_MIN_DEGREE || angle > SERVO_MAX_DEGREE) {
        return ESP_ERR_INVALID_ARG;
    }
    servo->current_angle = angle;
    return mcpwm_comparator_set_compare_value(servo->comparator, angle_to_compare(angle));
    return ESP_OK;
}

static void calc_angle(double x, double y, double *front_angle, double *rear_angle) {
    double front_hypo, front_height;
    double rear_hypo, rear_height;
    double theta_1, theta_2;
    double front_semiP, rear_semiP;
    double front_area, rear_area;

    // Step 1: Calculate Hypotenuses
    front_hypo = sqrt((x*x) + (y*y));
    rear_hypo = sqrt(pow(x-REAR_OFFSET, 2) + (y*y));

    // Step 2a: Calulate semi-peremters
    front_semiP = (front_hypo + UPPER_LEG + LOWER_LEG)/2;
    rear_semiP = (rear_hypo + UPPER_LEG + LOWER_LEG)/2;
    // Step 2b: Calculate triangle areas
    front_area = sqrt(front_semiP*(front_semiP - UPPER_LEG)*(front_semiP - LOWER_LEG)*(front_semiP - front_hypo));
    rear_area = sqrt(rear_semiP*(rear_semiP - UPPER_LEG)*(rear_semiP - LOWER_LEG)*(front_semiP - rear_hypo));

    //Step 2c: Calculate Heights
    front_height = 2 * front_area / front_hypo;
    rear_height = 2 * rear_area / rear_hypo;

    // Step 3a: Calc Front Angle
    theta_1 = asin(front_height/UPPER_LEG);
    double min_upper = sqrt(pow(LOWER_LEG, 2) - pow(front_height, 2));
    if(min_upper > front_hypo) {theta_1 = PI - theta_1;}
    theta_2 = atan2(y,x);
    *front_angle = 180 - 180*(theta_1+theta_2)/PI - 45;

    // Step 3b: Calc Rear Angle
    theta_1 = asin(rear_height/UPPER_LEG);
    if(min_upper > front_hypo) {theta_1 = PI - theta_1;}
    theta_2 = atan2(y, REAR_OFFSET-x);
    *rear_angle = (180 - 180*(theta_1+theta_2)/PI) * (-1) + 45;
}

static esp_err_t init_servo(servo_config_t *servo, mcpwm_timer_handle_t *timer, int gpio_num, int clock_group) {

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

void app_main(void)
{
    double front_angle, rear_angle, x, y;
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer1 = NULL;
    mcpwm_timer_handle_t timer2 = NULL;
    mcpwm_timer_config_t timer1_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    mcpwm_timer_config_t timer2_config = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer1_config, &timer1));
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer2_config, &timer2));

    servo_config_t Servo1;
    servo_config_t Servo2;
    servo_config_t Servo3;
    servo_config_t Servo4;
    init_servo(&Servo1, &timer1, FRONT_LEFT_SERVO, 0);
    init_servo(&Servo2, &timer1, FRONT_RIGHT_SERVO, 0);
    init_servo(&Servo3, &timer2, BACK_LEFT_SERVO, 1);
    init_servo(&Servo4, &timer2, BACK_RIGHT_SERVO, 1);

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer1));
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer2));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer1, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer2, MCPWM_TIMER_START_NO_STOP));

    // set_servo_angle(&Servo1, -45);
    // set_servo_angle(&Servo3, 45);
    // vTaskDelay(pdMS_TO_TICKS(3000));
    // set_servo_angle(&Servo1, 90);
    // set_servo_angle(&Servo3, -90);
    // vTaskDelay(pdMS_TO_TICKS(500));
    // set_servo_angle(&Servo1, 0);
    // set_servo_angle(&Servo3, 0);
    // ESP_LOGI(TAG, "Setup.....");
    // vTaskDelay(pdMS_TO_TICKS(3000));

    while(1) {
        x = 12; y = 5;
        // calc_angle(x, y, &front_angle, &rear_angle);
        // set_servo_angle(&Servo1, front_angle);
        // set_servo_angle(&Servo3, rear_angle);
        // ESP_LOGI(TAG, "X = %f, Y = %f, front angle = %f, rear angle = %f", x, y, front_angle, rear_angle);
        // vTaskDelay(pdMS_TO_TICKS(2000));

        // x = 12; y = 53;
        // calc_angle(x, y, &front_angle, &rear_angle);
        // set_servo_angle(&Servo1, front_angle);
        // set_servo_angle(&Servo3, rear_angle);
        // ESP_LOGI(TAG, "X = %f, Y = %f, front angle = %f, rear angle = %f", x, y, front_angle, rear_angle);
        // vTaskDelay(pdMS_TO_TICKS(2000));

        for(int i = 5; i < 53; i++) {
            x = 12;
            y = i;
            calc_angle(x, y, &front_angle, &rear_angle);
            set_servo_angle(&Servo2, front_angle);
            set_servo_angle(&Servo1, front_angle);
            set_servo_angle(&Servo4, rear_angle);
            set_servo_angle(&Servo3, rear_angle);
            ESP_LOGI(TAG, "X = %f, Y = %f, front angle = %f, rear angle = %f", x, y, front_angle, rear_angle);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));

        for(int i = 53; i > 5; i--) {
            x = 12;
            y = i;
            calc_angle(x, y, &front_angle, &rear_angle);
            set_servo_angle(&Servo2, front_angle);
            set_servo_angle(&Servo1, front_angle);
            set_servo_angle(&Servo4, rear_angle);
            set_servo_angle(&Servo3, rear_angle);
            ESP_LOGI(TAG, "X = %f, Y = %f, front angle = %f, rear angle = %f", x, y, front_angle, rear_angle);
            vTaskDelay(pdMS_TO_TICKS(50));
        }


        
    }
}
