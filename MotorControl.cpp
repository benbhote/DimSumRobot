#include "Config.h"
#include "MotorControl.h"

void initMotorsPins() {
  pinMode(pinPWMA, OUTPUT);
  pinMode(pinAIN1, OUTPUT);
  pinMode(pinAIN2, OUTPUT);
  pinMode(pinPWMB, OUTPUT);
  pinMode(pinBIN1, OUTPUT);
  pinMode(pinBIN2, OUTPUT);
  //pinMode(pinSTBY, OUTPUT); //Not enough pin to drive StandBy of TB6612FNG
}

void stopMotors() {
  analogWrite(pinPWMA, 0);
  analogWrite(pinPWMB, 0);
  digitalWrite(pinAIN1, LOW);
  digitalWrite(pinAIN2, LOW);
  digitalWrite(pinBIN1, LOW);
  digitalWrite(pinBIN2, LOW);
}

void controlMotorA(int pwm) {
  if (pwm >= 0) {
    digitalWrite(pinAIN1, HIGH);
    digitalWrite(pinAIN2, LOW);
  } else {
    digitalWrite(pinAIN1, LOW);
    digitalWrite(pinAIN2, HIGH);
    pwm = -pwm;
  }
  analogWrite(pinPWMA, constrain(pwm, 0, 255));
}

void controlMotorB(int pwm) {
  if (pwm >= 0) {
    digitalWrite(pinBIN1, LOW);
    digitalWrite(pinBIN2, HIGH);
  } else {
    digitalWrite(pinBIN1, HIGH);
    digitalWrite(pinBIN2, LOW);
    pwm = -pwm;
  }
  analogWrite(pinPWMB, constrain(pwm, 0, 255));
}

void controlMotorsExploration(int throttle, int steering) {
  // Mixing of Throttle (moving forward/backward) and Steering (turning left/right) - with adjustment to keep maniability
  int pwmLeft = throttle + steering;
  int pwmRight  = throttle - steering;

  controlMotorA(pwmLeft);
  controlMotorB(pwmRight);
}