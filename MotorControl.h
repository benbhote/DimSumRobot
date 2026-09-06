#ifndef MOTORS_CONTROLLER_H
#define MOTORS_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>

void initMotorsPins();
void stopMotors();
void controlMotorA(int pwm);
void controlMotorB(int pwm);
void controlMotorsExploration(int throttle, int steering);

#endif