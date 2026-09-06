#include "Config.h"
#include "CameraManager.h"

//Hardware state variables initialization
bool statusCamera = false;

// Camera pins
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39
#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

//Initialization function depending on the profil desired
bool cameraInit(ProfilCamera profil) {
  static bool cameraActive = false;

  if (cameraActive) {
    esp_camera_deinit();
    cameraActive = false;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  
  config.pixel_format = PIXFORMAT_JPEG;

  // Parameters depending on profil selected
  switch (profil) {
    case PROFIL_STANDARD:
      config.xclk_freq_hz = 20000000;
      config.frame_size = FRAMESIZE_QVGA;
      config.jpeg_quality = 16;
      config.fb_count = 3;
      break;

    case PROFIL_HD_QXGA:
      config.xclk_freq_hz = 20000000;
      config.frame_size = FRAMESIZE_QXGA;
      config.jpeg_quality = 4;
      config.fb_count = 1;
      break;

    case PROFIL_ASTRO_QXGA:
      config.xclk_freq_hz = 4000000;
      config.frame_size = FRAMESIZE_QXGA;
      config.jpeg_quality = 4;
      config.fb_count = 1;
      break;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("[CAMERA] Critical initialization error");
    return false;
  }

  // Correction of picture orientation
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 0);
    s->set_hmirror(s, 1);

    // Sensor tuning if QXGA
    if (profil == PROFIL_HD_QXGA) {
      s->set_gain_ctrl(s, 1);    
      s->set_exposure_ctrl(s, 1); 
      s->set_agc_gain(s, 30);
      s->set_aec_value(s, 600);
    } else if (profil == PROFIL_ASTRO_QXGA) {
      s->set_gain_ctrl(s, 0);  
      s->set_agc_gain(s, 0);   
      s->set_whitebal(s, 0);   
      s->set_exposure_ctrl(s, 0); 
      s->set_aec_value(s, 1200);  
      s->set_lenc(s, 0);       
    }
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  emptyCameraBuffer();
  cameraActive = true;
  return true;
}

//Buffer unload loop
void emptyCameraBuffer() {
  for (int i = 0; i < 3; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    } else {
      break;
    }
  }
}

// --- Central image capturing function ---
bool getPhoto(ProfilCamera profil) {
  // 1. Initialization with profil requested (HD or Astro)
  if (!cameraInit(profil)) return false;

  // 2. Light accumulation pause (only for Astro)
  if (profil == PROFIL_ASTRO_QXGA) {
    for (int i = 0; i < 5; i++) {
      vTaskDelay(pdMS_TO_TICKS(100));
      yield(); 
    }
  }

  // 3. File name and save
  String prefixeFile = (profil == PROFIL_ASTRO_QXGA) ? "Astro" : "Photo";
  bool saveStatus = saveImageSD(prefixeFile);

  // 4. Back to low quality profil (QVGA) to reduce system workload
  cameraInit(PROFIL_STANDARD);
  
  if (saveStatus) {
    Serial.println("[SYSTEM] Screenshot successful.");
  } else {
    Serial.println("[SYSTEM] Failed screenshot (SD write error).");
  }

  return saveStatus;
}

bool saveImageSD(String prefixe) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAMERA] Failed screenshot.");
    cameraInit(PROFIL_STANDARD); // Reset to QVGA
    return false;
  }
  String filePath = "/" + prefixe + "_" + String(millis()) + ".jpg";  
  File imageSD = SD.open(filePath.c_str(), FILE_WRITE);
  if (!imageSD) {
    Serial.println("[SD] Access denied.");
    esp_camera_fb_return(fb);
    cameraInit(PROFIL_STANDARD); // Reset to QVGA
    return false;
  }

  imageSD.write(fb->buf, fb->len);
  yield();
  imageSD.close();

  Serial.printf("[SD] File %s saved - Size : %u octets\n", prefixe.c_str(), fb->len);
  esp_camera_fb_return(fb);
  return true;
}

bool initCameraDefault() {
  if (cameraInit(PROFIL_STANDARD)) {
    statusCamera = true;
    Serial.println("[CAMERA] Camera initialization successful !");
    return true;
  } else {
    statusCamera = false;
    Serial.println("[CAMERA] Failed to initialize camera.");
    return false;
  }
}