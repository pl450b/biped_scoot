#include "driver/mcpwm_prelude.h"

typedef struct {
    mcpwm_oper_handle_t oper;
    mcpwm_operator_config_t oper_config;

    mcpwm_cmpr_handle_t comparator;
    mcpwm_comparator_config_t comparator_config;

    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t generator_config;

    int current_angle;
    int angle_offset;
} servo_config_t;

typedef struct {
    servo_config_t front_servo;
    servo_config_t rear_servo;
    mcpwm_timer_handle_t timer;
    bool left_leg;
} leg_t;

static inline uint32_t angle_to_compare(int angle);

void calc_angle(int x, int y, int *front_angle, int *rear_angle);

esp_err_t init_servo(servo_config_t *servo, mcpwm_timer_handle_t *timer, int gpio_num, int clock_group);

esp_err_t init_legs(leg_t* left_leg, leg_t* right_leg);

// Function to set servo angle
esp_err_t set_servo_angle(servo_config_t *servo, int angle);

esp_err_t set_leg_pos(leg_t* leg, int x, int y);
