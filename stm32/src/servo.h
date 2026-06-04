#ifndef __SERVO_H__
#define __SERVO_H__
#include <Arduino.h>

typedef struct{
    int8_t pin;
    int16_t mid_pulseWidth;
    int16_t cur_pulseWidth;
    int16_t target_pulseWidth;
    int32_t delay_time;
    bool moving;
    int32_t move_step_delay;
    int32_t last_move_time;
    int8_t move_direction;
}servo_t;

void servo_init();
void setServoAngle(int id, int angle, int time);
#endif