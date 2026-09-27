#ifndef MOTORS_CONTROLLER_H
#define MOTORS_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>

// Music Score management
struct DualNote {
    uint16_t freqA;     // Note frequency on motor A (0 = silence)
    uint16_t freqB;     // Note frequency on motor B (0 = silence)
    uint16_t duration;  // Duration of the note in milliseconds
};

//Music scores declarations
extern const DualNote bootMelody[];
extern const size_t bootMelodyLength;
extern const DualNote ascendingScale[];
extern const size_t ascendingScaleLength;
extern const DualNote descendingScale[];
extern const size_t descendingScaleLength;
extern const DualNote pictureTaken[];
extern const size_t pictureTakenLength;
extern const DualNote lowBatteryAlert[];
extern const size_t lowBatteryAlertLength;

void initMotorsPins();
void stopMotors();
void controlMotorA(int pwm);
void controlMotorB(int pwm);
void controlMotorsExploration(int throttle, int steering);
void playDualMelody(const DualNote* melody, size_t length);

#endif