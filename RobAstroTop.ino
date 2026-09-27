#include "Config.h"
#include "CameraManager.h"
#include "MotorControl.h"
#include "SensorsManager.h"
#include "WebServer.h"

#define TIMEOUT_EQUILIBRIUM_MS 3000 //Equilibrium duration

// WiFi credentials -- Wifi will be created on the ESP32-S3. Change the SSID and Password to something of your liking
const char *ssid = "DimSumRobot";
const char *password = "MoonSmile";

// Running WebServer on port 80
AsyncWebServer server(80);

// Mutex initialization
volatile bool isRequesting = false;
// Task instance
volatile TaskQueue taskRequested = NO_TASK;

// Default mode
Mode currentMode = GHOST;

// Shared variables to store data from Battery
float batteryVoltage = 0.0;
int batteryPercent = 0;
unsigned long lastTimeBatCheck = 0;
bool batteryLowSafety = false;
const unsigned long INTERVAL_BAT_MS = 5000; //Time between two reading of battery power

// Shared variables to store data from SHT40
float temperatureData = 0.0;
float humidityData = 0.0;
unsigned long lastTimeSHT = 0;
const unsigned long INTERVAL_SHT_MS = 15000; //Time between two reading call of readSensorSHT40

// Exploration mode variables
bool getMoveOrder = false;
static unsigned long startTempo = 0;
static bool activeTempo = false;
static bool motorsOff = true;
bool modeEquilibrium = false;
const int OFFSET_MIN = 35;
const int PWM_MAX = 200;
int consigneThrottle = 0;
int consigneSteering = 0;
TaskHandle_t radarTaskHandle = NULL;
volatile int distanceObstacle = 999;
volatile bool dangerObstacle = false;
TaskHandle_t explorationTaskHandle = NULL;
unsigned long timestampJoystick = 0;

void setup() {
  Serial.begin(115200);

  // 0. I2C bus initialization
  Wire.begin();

  // 1. Hardware modules initialization
  initMotorsPins();
  initCameraDefault();
  initADS1115();
  initSDCard();
  initRadar();
  initMPU6050();

  // 2. Wi-Fi access point launch
  WiFi.softAP(ssid, password, 1, 0, 1);
  Serial.println("[WIFI] WiFi launched (limited to 1 device).");

  // 3. Async Web server launch
  configWebServer(server);

  // 4. Radar task creation
  xTaskCreatePinnedToCore(
    radarTask,          // Function
    "RadarTask",        // Name
    4096,               // Stack
    NULL,               // Parametre
    1,                  // Priority (higher number means higher priority)
    &radarTaskHandle,   // Handle
  1                     // Core (o or 1)
  );

  // 5. Exploration task creation
  xTaskCreatePinnedToCore(
    cycleExplorationTask,
    "ExplorationTask",
    4096,
    NULL,
    2,
    &explorationTaskHandle,
    1
  );

  enterModeGhost();
  playDualMelody(bootMelody, bootMelodyLength); // Boot song
}

void loop() {
  static Mode oldMode = GHOST;
  int nbClients = WiFi.softAPgetStationNum();

  // --- CENTRAL SAFETY : if mode change, immediatly shutting off the motors ---
  if (currentMode != oldMode) {
    stopMotors();
    oldMode = currentMode;
  }

  // If no device connected, back to GHOST mode
  if (nbClients == 0 && (currentMode == DIAGNOSTIC || currentMode == OBSERVATION || currentMode == EXPLORATION || currentMode == GALLERY)) {
    Serial.println("No connection detected. Back to GHOST mode.");
    //Shutting off motors (again maybe) to be sure the robot is not moving
    stopMotors();
    enterModeGhost();
    playDualMelody(descendingScale,descendingScaleLength); //Song when entering GHOST mode
  }

    // --- Battery management ---
  if (currentMode != GHOST && (millis() - lastTimeBatCheck > INTERVAL_BAT_MS)) {
    lastTimeBatCheck = millis();
    readA0BatteryADS1115(batteryVoltage, batteryPercent);
    if (batteryLowSafety) {
      if (currentMode != DIAGNOSTIC){
        stopMotors();
        enterModeDiagnostic();
        events.send("battery_low_safety", "system_state", millis());
        playDualMelody(lowBatteryAlert,lowBatteryAlertLength); //Song alert for low battery
      }
    }
  }

  // --- SHT40 & battery management ---
  bool blockSHT = (modeEquilibrium && (!motorsOff || activeTempo)); //Exist condition to avoid I2C bus interference
  if (currentMode != GHOST && !blockSHT && (millis() - lastTimeSHT > INTERVAL_SHT_MS)) {
    lastTimeSHT = millis();
    readSensorSHT40(temperatureData, humidityData);
    //Serial.print("[SHT40] Temp : "); Serial.print(temperatureData); Serial.println(" °C | ");
    //Serial.print("Humidity : "); Serial.print(humidityData); Serial.println(" %");
  }

  switch (currentMode) {
    case GHOST:
      if (nbClients > 0) {
        Serial.println("Connection established ! Leaving GHOST mode.");
        enterModeDiagnostic();
        playDualMelody(ascendingScale,ascendingScaleLength); //Song when leaving GHOST mode 
      } else {
        delay(500); 
      }
      break;

    case DIAGNOSTIC:
      vTaskDelay(pdMS_TO_TICKS(100));
      break;

    case OBSERVATION:
      vTaskDelay(pdMS_TO_TICKS(100));
      break;

    case EXPLORATION:
      manageModeExploration();
      break;
    
    case GALLERY:
      vTaskDelay(pdMS_TO_TICKS(500));
      break;
  }

  // Heavy requests management
  if (isRequesting && taskRequested != NO_TASK) {
    bool photoSuccess = false; //Variable to return state of photo's save to play correct song
    
    switch (taskRequested) {
      case CAPTURE_ASTRO:
        photoSuccess = getPhoto(PROFIL_ASTRO_QXGA);
        break;              
      case CAPTURE_HD:
        photoSuccess = getPhoto(PROFIL_HD_QXGA);
        break;              
      default:
        break;
    }
    //Managing result of photo capture
    if (taskRequested == CAPTURE_ASTRO || taskRequested == CAPTURE_HD) {
      if (photoSuccess) {
        playDualMelody(pictureTaken, pictureTakenLength); // Photo taken song
      } else {
        events.send("photo_failed", "system_state", millis()); // Fail event
      }
    }
    // Finalizing task
    taskRequested = NO_TASK;
    isRequesting = false; // Unlocking
  }
}

void enterModeGhost() {
  currentMode = GHOST;
  isRequesting = false; // Safety reset of the variable in case of lost connection for example
  Serial.println("-> GHOST mode : waiting for connection.");
}

void enterModeDiagnostic() {
  vTaskDelay(pdMS_TO_TICKS(10));
  currentMode = DIAGNOSTIC;
  Serial.println("-> DIAGNOSTIC mode.");
}

void enterModeObservation() {
  vTaskDelay(pdMS_TO_TICKS(10));
  currentMode = OBSERVATION;
  Serial.println("-> OBSERVATION mode.");
}

void enterModeExploration() {
  vTaskDelay(pdMS_TO_TICKS(10));
  currentMode = EXPLORATION;
  initExploration();
  Serial.println("-> EXPLORATION mode.");
}

void enterModeGallery() {
  vTaskDelay(pdMS_TO_TICKS(10));
  currentMode = GALLERY;
  Serial.println("-> GALLERY mode.");
}

// --- Web functions (to get captors values) ---

int webRadarDistanceStr() {
  int distance = getRadarDistanceStr();
  if (distance != -1) {
    statusRadar = true;
  } else {
    statusRadar = false;
  }
  return distance; 
}

bool webMpuData(float &gravity, float &pitch, float &roll) {
  if (!statusMPU) return false;
  
  readMPUData();
  gravity = (float)currentAx / 16384.0f; // Converting in g (range ±2g)
  getPitch();
  getRoll();
  
  pitch = anglePitch;
  roll = angleRoll;
  return true;
}

// --- Exploration mode functions ---

void initExploration() {
  startTempo = 0;
  activeTempo = false;
  motorsOff = true;
  modeEquilibrium = false;
  stopMotors();
}

void radarTask(void *pvParameters) {
  while (true) {
    // 1. Task sleeping until getting a signal to wake up
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 

    // 2. When awake, calculate distance each 50ms and update flag obstacle
    while (getMoveOrder && statusRadar) {
      distanceObstacle = getRadarDistanceStr();
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 3. When robot stop moving, set distance back to 999
    distanceObstacle = 999;
  }
}

void cycleExplorationTask(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10);

    for (;;) {
        // 1. Task sleeping until getting a signal to wake up
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 2. Reset FilterTimestamp to avoid absurd value due to long period between robot boot and call of the function
        resetFilterTimestamp();

        // 3. When awake, first set PID timing
        xLastWakeTime = xTaskGetTickCount();

        // 4. To maintain equilibrium, loop at 100Hz frequency
        while (modeEquilibrium) {
            cycleEquilibrium();

            // If robot stop moving and timer done, exit the loop
            if (!getMoveOrder && motorsOff) {
                break;
            }

            // Keep a precise 10ms between cycle
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
        }
    }
}

int manageThrottle(int inputThrottle) {
    const int maxInputRange = PWM_MAX - OFFSET_MIN;
    int targetThrottle = 0;
    int baseThrottle = 0;

    if(!modeEquilibrium) {
      // Manual cycle
      // 1. Motor deadband management
      if (abs(inputThrottle) > 2) {
          int sign = (inputThrottle > 0) ? 1 : -1;
          
          // Normalized input between 0.0 and 1.0
          float normalizedInput = (float)abs(inputThrottle) / (float)maxInputRange;
          if (normalizedInput > 1.0f) normalizedInput = 1.0f;

          // Logarithmic function application (k = curvature parameter)
          float k = 5.0f; 
          float logValue = log(1.0f + k * normalizedInput) / log(1.0f + k);

          // Back to scale with integration of offset value
          int scaledMagnitude = OFFSET_MIN + (int)(logValue * (PWM_MAX - OFFSET_MIN));
          
          targetThrottle = sign * scaledMagnitude;
      } else {
          targetThrottle = 0;
      }

      // 2. Variation limiter (Smoothing / MaxStep)
      static int previousBaseThrottle = 0;
      const int maxStep = 15; 
      int delta = constrain(targetThrottle - previousBaseThrottle, -maxStep, maxStep);
      baseThrottle = previousBaseThrottle + delta;
      previousBaseThrottle = baseThrottle; 
    }
    else {
        // Equilibrium cycle
        if (abs(inputThrottle) > 2) {
            int sign = (inputThrottle > 0) ? 1 : -1;
            if (abs(inputThrottle) < OFFSET_MIN) {
                baseThrottle = sign * OFFSET_MIN;
            } else {
                baseThrottle = inputThrottle;
            }
        } else {
            baseThrottle = 0;
        }
    }

    // 3. PWM safety constrain
    return constrain(baseThrottle, -PWM_MAX, PWM_MAX);
}

void cycleManual() {
  if (getMoveOrder) {
    activeTempo = false;
    motorsOff = false;
    int baseThrottle = manageThrottle(consigneThrottle);
    controlMotorsExploration(baseThrottle, consigneSteering);
  } else {
    stopMotors();
    motorsOff = true;
  }
}

void cycleEquilibrium () {
  static float sumErrors = 0.0;
  static float previousError = 0.0;
  const float dt = 0.01f; // Fix dt due to 10ms cycle of cycleExplorationTask
  const float PITCH_OFFSET = -1.8f; //Adjust to reflete the true balanced angle of the robot depending on your MPU's fixation

  // MPU read with safety max tilt angle (> 30°)
  checkAndGetMPU();
  if (abs(anglePitch) > 30.0) {
      stopMotors();
      motorsOff = true;
      activeTempo = false;
      sumErrors = 0.0;
      previousError = 0.0;
      return; 
  }

  // Timer management (timeout of 3 secondes without moving order)
  if (getMoveOrder) {
      activeTempo = false;
      motorsOff = false;
  } 
  else {
      if (motorsOff) return;

      if (!activeTempo) {
          startTempo = millis();
          activeTempo = true;
          //Serial.println("1 -> Début Sablier");    
      }

      if ((millis() - startTempo) >= TIMEOUT_EQUILIBRIUM_MS) {
          stopMotors();
          motorsOff = true;
          activeTempo = false;
          sumErrors = 0.0;
          //Serial.println("2 -> Fin Sablier");
          return;
      }
  }

  // --- PID calculation ---
  float targetAngle = PITCH_OFFSET + (getMoveOrder ? ((float)consigneThrottle * 0.05f) : 0.0f);
  float error = anglePitch - targetAngle;

  //If needed, a deadband angle can be added but it's not recommended for a self-balancing robot
  //if (abs(error) < 1.0f) error = 0.0f;

  sumErrors += error * dt;
  sumErrors = constrain(sumErrors, -20.0, 20.0); 

  float deltaError = (error - previousError) / dt;
  previousError = error;

  // PID constants
  const float Kp = 15.0;  // More or less PWM depending on the angle
  const float Ki = 0.5;   // Builds up strength  if robot get stuck
  const float Kd = 1.5;   // Slow down robot when approaching equilibrium

  float realOrder = (Kp * error) + (Ki * sumErrors) + (Kd * deltaError);
  int rawThrottle = -(int)realOrder; 

  // PID specificity : reset integrator if throttle in deadzone
  if (rawThrottle >= -2 && rawThrottle <= 2) {
      sumErrors = 0.0;
  }
  
  // Unified processing application (Offset + Smoothing + PWM safety constrain)
  int baseThrottle = manageThrottle(rawThrottle);

  if (getMoveOrder) {
      controlMotorsExploration(baseThrottle, consigneSteering);
  } else {
      controlMotorsExploration(baseThrottle, 0); // Equilibrium during timer
  }
}

void manageModeExploration() {
  // 1. Absolute priority : global shut down
  if (emergencyStop && modeEquilibrium) {
      stopMotors();
      events.send("mpu_ko", "system_state", millis());
      vTaskDelay(pdMS_TO_TICKS(20));
      modeEquilibrium = false;
      return;
  }

  // --- WATCHDOG JOYSTICK (Anti signal loss safety) ---
  if (getMoveOrder && (millis() - timestampJoystick > 250)) {
      consigneThrottle = 0;
      consigneSteering = 0;
      getMoveOrder = false;
      stopMotors();
      motorsOff = true; // Stop even if equilibrium cycle is active      
  }

  // 2. Radar anti collision safety
  if (statusRadar) {
      if (distanceObstacle > 0) { // Distance value exploitable
          if (distanceObstacle < 20) {
              // Less than 20 cm : only backward allowed
              if (consigneThrottle > 0) {
                  consigneThrottle = 0;
              }
          } 
          else if (distanceObstacle < 50) {
              // Between 20 cm and 50 cm : 30% of PWM max
              int limitPwm = PWM_MAX * 0.3;
              if (consigneThrottle > limitPwm) {
                  consigneThrottle = limitPwm;
              }
          } 
          else if (distanceObstacle < 100) {
              // Between 50 cm and 1 m : 50% of PWM max
              int limitPwm = PWM_MAX * 0.5;
              if (consigneThrottle > limitPwm) {
                  consigneThrottle = limitPwm;
              }
          }
          // Otherwise, no limit other than PWM max
      }
  }

  // 3. Mode selector (manual or equilibrium)
  if (!modeEquilibrium) {
    cycleManual();
  }
  else {
  // If movement order and equilibrium task asleep, waking up task
    if (getMoveOrder || activeTempo || !motorsOff) {
        if (explorationTaskHandle != NULL) {
            xTaskNotifyGive(explorationTaskHandle);
        }
    } else {
        // If no movement order and no timer, stoping motors
        stopMotors();
        motorsOff = true;
    }
  }
}