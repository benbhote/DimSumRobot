#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD.h"

// Possible profils definition
enum ProfilCamera {
  PROFIL_STANDARD,
  PROFIL_HD_QXGA,
  PROFIL_ASTRO_QXGA
};

bool cameraInit(ProfilCamera profil);
void emptyCameraBuffer();
bool getPhoto(ProfilCamera profil);
bool saveImageSD(String prefixe);
bool initCameraDefault();

#endif