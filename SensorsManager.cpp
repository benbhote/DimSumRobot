#include "Config.h"
#include "SensorsManager.h"

//Hardware state variables initialization
bool statusSD = false;
bool statusMPU = false;
bool statusSHT40 = false;
bool statusRadar = false;

//Variables to display data on webserver
float anglePitch = 0.0;
float angleRoll = 0.0;

// Due to the orientation of MPU6050 inside the case : ax=gravity, ay=roll (left/right angle) and az=pitch (forward/backward angle)

// Accelerometer variables
int16_t currentAx = 0;
int16_t currentAy = 0;
int16_t currentAz = 0;

// Gyroscope variables
static int16_t currentGx = 0;
static int16_t currentGy = 0;
static int16_t currentGz = 0;

// MPU reset variables
static int consecutiveFailMPU = 0;
static int consecutiveFailReset = 0; 
const int MAX_RESETS_ALLOWED = 3;
bool emergencyStop = false;

// Filter variable
static unsigned long filterTimestamp = 0;

// --- SHT40 sensor function ---
bool readSensorSHT40(float &temp, float &hum) {
  Wire.beginTransmission(SHT40_ADDR);
  Wire.write(0xFD); 
  if (Wire.endTransmission() != 0) {
    statusSHT40 = false;
    return false; 
  } 

  vTaskDelay(pdMS_TO_TICKS(10));

  Wire.requestFrom(SHT40_ADDR, 6);
  if (Wire.available() < 6) {
    statusSHT40 = false;
    return false;
  }

  uint16_t t_ticks = (Wire.read() << 8) | Wire.read();
  Wire.read(); // CRC ignore
  uint16_t rh_ticks = (Wire.read() << 8) | Wire.read();
  Wire.read(); // CRC ignore

  temp = -45.0 + 175.0 * ((float)t_ticks / 65535.0);
  hum = -6.0 + 125.0 * ((float)rh_ticks / 65535.0);
  
  if (hum > 100.0) hum = 100.0;
  if (hum < 0.0) hum = 0.0;

  statusSHT40 = true;
  return true;
}

// --- microSD card function ---
bool initSDCard() {
  if (!SD.begin(21)) {
    Serial.println("[SD] Fail to initialized SD card (SPI) !");
    statusSD = false;
    return false;
  } else {
    Serial.println("[SD] SD card initialized via SPI.");
    statusSD = true;
    return true;
  }
}

// IRS function instantly activated via echo pin state change
volatile unsigned long tStartEcho = 0;
volatile unsigned long tEndEcho = 0;
volatile bool newDataAvailable = false;

void IRAM_ATTR ISR_ChangingEchoState() {
    if (digitalRead(pinRadar) == HIGH) {
        tStartEcho = micros();
    } else {
        tEndEcho = micros();
        newDataAvailable = true;
    }
}

// --- Radar functions ---
void initRadar() {
  pinMode(pinRadar, INPUT);
  //IRS attach
  attachInterrupt(digitalPinToInterrupt(pinRadar), ISR_ChangingEchoState, CHANGE);

  Serial.println("[RADAR] Radar pin initialized.");
  
  delay(100); // Break to let radar start

  int testDist = getRadarDistanceStr();
  if (testDist >= 0) {
    statusRadar = true;
    Serial.println("[RADAR] Sensor up and ready.");
  } else {
    statusRadar = false;
    Serial.println("[RADAR] Sensor not responding after boot.");
  }
}

int getRadarDistanceStr() { //Use of IRS function because pulseIn(pinRadar, HIGH, 12000) lock the core until it's done
  // 0. Radar emit and listen on the same pin, detach the pin to emit in OUTPUT
  detachInterrupt(digitalPinToInterrupt(pinRadar));
  // 1. Trig pulse
  pinMode(pinRadar, OUTPUT);
  digitalWrite(pinRadar, LOW);
  delayMicroseconds(2);
  digitalWrite(pinRadar, HIGH);
  delayMicroseconds(10); // Requirement of 10µs minimum from documentation 
  digitalWrite(pinRadar, LOW);

  // 2. Listening (Echo - INPUT)
  pinMode(pinRadar, INPUT);
  newDataAvailable = false;
  tStartEcho = 0;
  tEndEcho = 0;

  // 3. Attach the pin to listen the echo without locking the core
  attachInterrupt(digitalPinToInterrupt(pinRadar), ISR_ChangingEchoState, CHANGE);
  // Timeout 12ms (approximatly 2 meters)
  unsigned long startWait = millis();
  while (!newDataAvailable) {
      if (millis() - startWait > 12) {
          detachInterrupt(digitalPinToInterrupt(pinRadar));
          return -1; // No echo
      }
      vTaskDelay(pdMS_TO_TICKS(1)); // Very small break for CPU and PID
  }

  detachInterrupt(digitalPinToInterrupt(pinRadar));

  // Distance calculation
  if (tEndEcho > tStartEcho) {
      unsigned long duration = tEndEcho - tStartEcho;
      int distance = (int)(duration * 0.034 / 2);
      return distance;
  }

  return -1;
}

// --- MPU6050 functions ---
bool initMPU6050(bool forceReset) {
  // In case of problem, full reset
  if (forceReset) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0x80); // Internal hardware reboot command
    Wire.endTransmission();
    delay(100);       // Delay to allow internal reboot
  }

  // Classical wake up sequence
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B); 
  Wire.write(0x00); // Exiting sleeping mode
  if (Wire.endTransmission() != 0) {
    statusMPU = false;
    Serial.println("[MPU6050] I2C communication failed !");
    return false;
  }

  // Register configuration of ACCEL_CONFIG (±2g)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1C); 
  Wire.write(0x00); 
  Wire.endTransmission();

  statusMPU = true;
  Serial.println("[MPU6050] Successful initialization.");
  return true;
}

void checkAndGetMPU() {
  // 1. Check up of I2C communication and no value stuck at 0
  if (readMPUData() && !(currentAx == 0 && currentAy == 0 && currentAz == 0)) {
    consecutiveFailMPU = 0;
    consecutiveFailReset = 0; // Reset of failcounter
    getFilteredPitch();
  } 
  else {
    consecutiveFailMPU++;
    
    // If no value are send during 5 cycles (~50ms)
    if (consecutiveFailMPU > 5) {
      Serial.println("⚠️ MPU6050 communication lost, reset attempt...");
      
      if (initMPU6050(true)) {
        Serial.println("✅ MPU6050 back to work !");
        statusMPU = true;
        consecutiveFailReset = 0;
      } 
      else {
        Serial.println("❌ Failed to restore communication with MPU6050.");
        statusMPU = false;
        consecutiveFailReset++;
      }
      consecutiveFailMPU = 0; 
    }

    // 2. CRITICAL SAFETY : If the MPU6050 is still unavailable after 5 attempts
    if (consecutiveFailReset >= MAX_RESETS_ALLOWED) {
      Serial.println("🚨 Critical error : Impossible to communicate with MPU6050. Permanent shutdown of MPU !");
      statusMPU = false;
      emergencyStop = true;
    }
  }
}

bool readMPUData() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); // Beginning of acceleration value (ACCEL_XOUT_H)
  if (Wire.endTransmission(false) != 0) {
    statusMPU = false;
    return false; // I2C transmission failed
  }

  uint8_t bytesReceived = Wire.requestFrom(MPU6050_ADDR, 14, true);
  if (bytesReceived != 14) {
    statusMPU = false;
    return false; // Data incomplete
  }

  // 1. Accelerometer data reading
  currentAx = Wire.read() << 8 | Wire.read();
  currentAy = Wire.read() << 8 | Wire.read();
  currentAz = Wire.read() << 8 | Wire.read();
  currentAx = -currentAx; //Because the MPU is installed vertically, data needs to be corrected

  // 2. Temp data ignored (2 octets)
  Wire.read(); 
  Wire.read();

  // 3. Gyroscope data reading
  currentGx = (Wire.read() << 8) | Wire.read();
  currentGy = (Wire.read() << 8) | Wire.read();
  currentGz = (Wire.read() << 8) | Wire.read();
  currentGx = -currentGx; //Because the MPU is installed vertically, data needs to be corrected

  statusMPU = true;
  return true;
}

void getPitch() {
  anglePitch = atan2((float)currentAz, (float)currentAx) * 180.0 / PI;
}
void getRoll() {
  angleRoll = atan2((float)currentAy, (float)currentAx) * 180.0 / PI;
}

void getFilteredPitch() {
  // Converting delta temps (dt) in seconds
  unsigned long actualTime = millis();
  float dt = (actualTime - filterTimestamp) / 1000.0;
  if (dt <= 0.0) dt = 0.01; // Safety to avoid dividing by 0
  filterTimestamp = actualTime;

  // Brut angle brut from accelerometer
  float angleAccel = atan2((float)currentAz, (float)currentAx) * 180.0 / PI;

  // Gyroscope angular velocity converted to °/s 
  float gyroRate = (float)currentGz / 131.0; // Sensibility range ±250°/s

  // Complementary filter (98% Gyroscope, 2% Accelerometer)
  // Priority to the fluidity of gyroscope value to eliminate motors vibrations, with slight adjustment of accelerometer value to avoid drifting
  anglePitch = 0.98 * (anglePitch + gyroRate * dt) + 0.02 * angleAccel;
}