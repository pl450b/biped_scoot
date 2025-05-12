#ifndef LEGS_H
#define LEGS_H

#include "driver/mcpwm_prelude.h"


typedef struct {
    mcpwm_cmpr_handle_t comparator;
    mcpwm_comparator_config_t comparator_config;

    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t generator_config;

    int current_angle;
} servo_config_t;

typedef struct {
    mcpwm_oper_handle_t oper;
    mcpwm_operator_config_t oper_config;

    servo_config_t front_servo;
    servo_config_t rear_servo;

    mcpwm_timer_handle_t timer;
} leg_t;

#ifdef __cplusplus
extern "C" {
#endif

inline uint32_t angle_to_compare(int angle);

esp_err_t init_servo(servo_config_t *servo, mcpwm_oper_handle_t oper, int gpio_num);

esp_err_t init_leg(leg_t* leg, int gpio_pin_a, int gpio_pin_b, int clock_group);

esp_err_t set_servo_angle(servo_config_t* servo, int angle);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LEGS_H
