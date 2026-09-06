#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include "esp_camera.h"

bool isSystemBusy(AsyncWebServerRequest *request);
void configWebServer(AsyncWebServer &server);

#endif