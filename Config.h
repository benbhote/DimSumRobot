#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "esp_task_wdt.h" //Watchdog library
#include <ESPAsyncWebServer.h>

extern AsyncEventSource events;

extern bool emergencyStop;

// --- Mutex to manage processor workload ---
extern volatile bool isRequesting; // If needed, it is possible to use std::atomic<bool> (without forgetting to #include <atomic>) to ensure core treatment sync

// --- Current request variables
enum TaskQueue {
  NO_TASK,
  CAPTURE_ASTRO,
  CAPTURE_HD
  // More can be added if needed
};
extern volatile TaskQueue taskRequested; // Current request in memory -- if an parameter is necessary for a function, another varialbe must be declared (ex : extern String argumentTask;)

// --- Hardware state variable for Webserver ---
extern bool statusCamera;
extern bool statusSD;
extern bool statusMPU;
extern bool statusSHT40;
extern bool statusRadar;

// --- Motors pins ---
//Not enough pin to drive StandBy of TB6612FNG
//Left Motor
const int pinPWMA = 1; //D0
const int pinAIN1 = 2; //D1
const int pinAIN2 = 41; //MTDI - D12
//Right Motor
const int pinPWMB = 4; //D3
const int pinBIN1 = 43; //D6
const int pinBIN2 = 44; //D7

// --- Sensors pins ---
const int pinRadar = 42; //MTMS - D11

// --- I2C addresses---
const uint8_t MPU6050_ADDR = 0x68;
const uint8_t SHT40_ADDR = 0x44;

// --- Moving controls variables
extern bool modeEquilibrium;
extern bool getMoveOrder;
extern int consigneThrottle;
extern int consigneSteering;
extern TaskHandle_t radarTaskHandle;
extern unsigned long timestampJoystick;

// --- Shared variables (T° and Humidity) ---
extern float temperatureData;
extern float humidityData;

// --- Changing mode prototypes functions ---
void enterModeDiagnostic();
void enterModeObservation();
void enterModeExploration();
void enterModeGallery();

// --- Camera globals shared functions ---
void emptyCameraBuffer();

// --- Variables and gateways MPU/Radar shared for web server ---
extern float anglePitch;
extern float angleRoll;
extern int16_t currentAx;
extern int16_t currentAy;
extern int16_t currentAz;
int webRadarDistanceStr();
bool webMpuData(float &gravity, float &pitch, float &roll);

// --- Robot functionning mode ---
enum Mode { 
  GHOST,
  DIAGNOSTIC,
  OBSERVATION,
  EXPLORATION,
  GALLERY
  };

extern Mode currentMode;

#endif