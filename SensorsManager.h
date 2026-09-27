#ifndef SENSORS_MANAGER_H
#define SENSORS_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

extern float anglePitch;
extern float angleRoll;
extern int16_t currentAx;
extern int16_t currentAy;
extern int16_t currentAz;

bool initADS1115();
bool readA0BatteryADS1115(float &voltage, int &percent);
bool readSensorSHT40(float &temp, float &hum);
bool initSDCard();
void initRadar();
void ISR_ChangingEchoState();
int getRadarDistanceStr();
bool initMPU6050(bool forceReset = false);
void checkAndGetMPU();
bool readMPUData();
void getPitch();
void getRoll();
void resetFilterTimestamp();
void getFilteredPitch();

#endif