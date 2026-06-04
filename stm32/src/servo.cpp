#include "servo.h"
#include <Arduino.h>

static servo_t servos[6] = {
    {PC10, 75, 75, 75, 1000, false, 0, 0, 0},
    {PC11, 75, 75, 75, 1000, false, 0, 0, 0},
    {PC12, 70, 70, 70, 1000, false, 0, 0, 0},
    {PD2, 68, 68, 68, 1000, false, 0, 0, 0},
    {PB5, 70, 70, 70, 1000, false, 0, 0, 0},
    {PB8, 75, 75, 75, 1000, false, 0, 0, 0},
};

void servo_isr(){
  static int32_t cnt_50Hz = 0;
  cnt_50Hz++;
  if(cnt_50Hz >= 1000){//20ms
    cnt_50Hz = 0;
    int32_t current_time = millis();
    for(int i = 0; i < 6; i++){
      if(servos[i].moving) {
        // 检查是否到达目标位置
        if(servos[i].cur_pulseWidth == servos[i].target_pulseWidth) {
          servos[i].moving = false;
          continue;
        }
        
        // 检查移动时间间隔
        if(current_time - servos[i].last_move_time >= servos[i].move_step_delay) {
          // 执行单步移动
          servos[i].cur_pulseWidth += servos[i].move_direction;
          servos[i].last_move_time = current_time;
          
          // 检查是否到达或超出目标位置
          if((servos[i].move_direction > 0 && servos[i].cur_pulseWidth >= servos[i].target_pulseWidth) ||
             (servos[i].move_direction < 0 && servos[i].cur_pulseWidth <= servos[i].target_pulseWidth)) {
            servos[i].cur_pulseWidth = servos[i].target_pulseWidth;
            servos[i].moving = false;
          }
        }
      }
    }
  }
  
  for(int i = 0; i < 6; i++){
    if(cnt_50Hz < servos[i].cur_pulseWidth){
      digitalWrite(servos[i].pin, HIGH);
    }else{
      digitalWrite(servos[i].pin, LOW);
    }
  }
}
// static void servo_set_angle(int id, int angle){
//   servos[id].cur_pulseWidth = map(angle, 0, 180, 
//     servos[id].mid_pulseWidth-50, servos[id].mid_pulseWidth+50);
// }
// put function declarations here:
void servo_init(){
  for(int i = 0; i < 6; i++){
    pinMode(servos[i].pin, OUTPUT);
    servos[i].cur_pulseWidth = servos[i].mid_pulseWidth;
    servos[i].target_pulseWidth = servos[i].mid_pulseWidth;
    servos[i].delay_time = 1000;
  }

  Timer3.pause();
  Timer3.setPrescaleFactor(72); // 72MHz / 72 = 1MHz
  Timer3.setOverflow(20); // 1MHz / 20 = 50kHz(20us)
  Timer3.setChannel1Mode(TIMER_OUTPUT_COMPARE);
  Timer3.setCompare(TIMER_CH1,1);
  Timer3.attachCompare1Interrupt(servo_isr);
  Timer3.refresh();
  Timer3.resume();
}

void setServoAngle(int servo_id, int angle, int delay_time){
    if(servo_id < 0 || servo_id > 5){
        return;
    }
    if(angle < 0){
        angle = 0;
    }
    if(angle > 180){
        angle = 180;
    }
    servos[servo_id].target_pulseWidth = map(angle, 0, 180,
                                           servos[servo_id].mid_pulseWidth-50,
                                           servos[servo_id].mid_pulseWidth+50);
  
    int32_t diff = servos[servo_id].target_pulseWidth - servos[servo_id].cur_pulseWidth;
  if(diff == 0) {
    servos[servo_id].moving = false;
    return;
  }
  
  servos[servo_id].move_direction = (diff > 0) ? 1 : -1;
  int32_t steps = abs(diff);
  servos[servo_id].move_step_delay = (steps > 0) ? delay_time / steps : 0;
  servos[servo_id].moving = true;
  servos[servo_id].last_move_time = millis();
}