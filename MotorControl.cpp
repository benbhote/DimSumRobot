#include "Config.h"
#include "MotorControl.h"

// LEDC channels declaration
const uint8_t CHANNEL_MOTOR_A = 0;
const uint8_t CHANNEL_MOTOR_B = 1;
const uint32_t PWM_FREQ_BASE = 20000; // Default motors frequency (20 kHz)
const uint8_t  PWM_RESOLUTION = 8;   // 8 bits resolution (0 to 255)

//-- Boot song
const DualNote bootMelody[] = {
    {220, 277, 100}, // A3 / C#4
    {330, 415, 100}, // E4 / G#4
    {330, 349, 100}, // E4 / F4
    {208, 247, 100}, // G#3 / B3
    {220, 277, 100}  // A3 / C#4
};
const size_t bootMelodyLength = sizeof(bootMelody) / sizeof(bootMelody[0]);

//-- Leaving GHOST mode song
const DualNote ascendingScale[] = {
    {523,  493,  40},    // C5 / B4
    {622,  587,  40},   // Eb5 / D5
    {740,  698,  40},   // F#5 / F5
    {880,  830,  40},   // A5 / Ab5
    {1046, 988,  40},   // C6 / B5
    {1245, 1175, 40},   // Eb6 / D6
    {1480, 1397, 40},   // F#6 / F6
    {1760, 1661, 40},   // A6 / Ab6
    {2093, 1975, 60}   // C7 / B6
};
const size_t ascendingScaleLength = sizeof(ascendingScale) / sizeof(ascendingScale[0]);

//-- Entering GHOST mode song
const DualNote descendingScale[] = {
    {2093, 1975, 40},   // C7 / B6
    {1760, 1661, 40},   // A6 / Ab6
    {1480, 1397, 40},   // F#6 / F6
    {1245, 1175, 40},   // Eb6 / D6
    {1046, 988,  40},   // C6 / B5
    {880,  830,  40},   // A5 / Ab5
    {740,  698,  40},   // F#5 / F5
    {622,  587,  40},   // Eb5 / D5
    {523,  493,  60}    // C5 / B4
};
const size_t descendingScaleLength = sizeof(descendingScale) / sizeof(descendingScale[0]);

//-- Photo Song
const DualNote pictureTaken[] = {
    {523, 659, 70},   // C5 / E5
    {659, 784, 70},   // E5 / G5
    {784, 1046, 70},  // G5 / C6
    {1046, 1318, 150} // C6 / E6
};
const size_t pictureTakenLength = sizeof(pictureTaken) / sizeof(pictureTaken[0]);

//-- Low battery song
const DualNote lowBatteryAlert[] = {
    {300, 250, 200},  // D4 (approx.) / B3
    {200, 150, 400}   // G3 / D#3 (approx.)
};
const size_t lowBatteryAlertLength = sizeof(lowBatteryAlert) / sizeof(lowBatteryAlert[0]);

// Functions
void initMotorsPins() {
  ledcAttachChannel(pinPWMA, PWM_FREQ_BASE, PWM_RESOLUTION, CHANNEL_MOTOR_A);
  pinMode(pinAIN1, OUTPUT);
  pinMode(pinAIN2, OUTPUT);
  ledcAttachChannel(pinPWMB, PWM_FREQ_BASE, PWM_RESOLUTION, CHANNEL_MOTOR_B);
  pinMode(pinBIN1, OUTPUT);
  pinMode(pinBIN2, OUTPUT);
  //pinMode(pinSTBY, OUTPUT); //Not enough pin to drive StandBy of TB6612FNG (solution : connect StandBy to 3V3)
}

void stopMotors() {
  ledcWrite(pinPWMA, 0);
  ledcWrite(pinPWMB, 0);
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
  ledcWrite(pinPWMA, constrain(pwm, 0, 255));
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
  ledcWrite(pinPWMB, constrain(pwm, 0, 255));
}

void controlMotorsExploration(int throttle, int steering) {
  // Mixing of Throttle (moving forward/backward) and Steering (turning left/right) - with adjustment to keep maniability
  int pwmLeft = throttle + steering;
  int pwmRight  = throttle - steering;

  int maxVal = max(abs(pwmLeft), abs(pwmRight));
  if (maxVal > 255) {
    pwmLeft = (pwmLeft * 255) / maxVal;
    pwmRight = (pwmRight * 255) / maxVal;
  }

  controlMotorA(pwmLeft);
  controlMotorB(pwmRight);
}

void playDualMelody(const DualNote* melody, size_t length) {
  // Putting motor out of brake mode 
  digitalWrite(pinAIN1, HIGH);
  digitalWrite(pinAIN2, LOW);
  digitalWrite(pinBIN1, HIGH);
  digitalWrite(pinBIN2, LOW);

  // Loop to play each note of the melody
  for (size_t i = 0; i < length; i++) {
    // Motor A music note management
    if (melody[i].freqA > 0) {
        ledcChangeFrequency(pinPWMA, melody[i].freqA, 8);
        ledcWrite(pinPWMA, 5); // ~2% duty cycle to make the motor sing
    } else {
        ledcWrite(pinPWMA, 0);   // Silence
    }

    // Motor B music note management
    if (melody[i].freqB > 0) {
        ledcChangeFrequency(pinPWMB, melody[i].freqB, 8);
        ledcWrite(pinPWMB, 5); // ~2% duty cycle to make the motor sing
    } else {
        ledcWrite(pinPWMB, 0);   // Silence
    }

    // Duration of the note
    delay(melody[i].duration);
  }

  // End of song : stop motors and back to 20kHz frequency
  stopMotors();
  ledcChangeFrequency(pinPWMA, PWM_FREQ_BASE, PWM_RESOLUTION);
  ledcChangeFrequency(pinPWMB, PWM_FREQ_BASE, PWM_RESOLUTION);
}