
#include <Arduino.h>
#include <esp_log.h>
#include <TFT_eSPI.h>
#include <freertos/queue.h>
#include "esp_camera.h"

static const char *TAG = "cameraTask";

extern TFT_eSPI tft;
extern sensor_t *sensor;
extern QueueHandle_t frameQueue;
extern int special;
extern int special2;

esp_err_t cameraTask_Init();
void cameraTask(void *pvParameters);
void cameraTask_InitCameraSoftwareConfig();