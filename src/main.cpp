#include <Arduino.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include <esp_task_wdt.h>
#include "cameraTask.h"
#include "displayTask.h"
#include "config.h"

SemaphoreHandle_t camMutex;
TaskHandle_t cameraTaskHandle = NULL; // 声明全局句柄变量

void setup()
{
  ESP_LOGI(TAG, "setup start......");
  pinMode(FLASH_LIGHT_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  
  digitalWrite(FLASH_LIGHT_PIN, LOW);

  Serial.begin(115200);
  delay(1000);
  camMutex = xSemaphoreCreateMutex();
  displayTask_Init();
  cameraTask_Init();

  // Core 0绑定摄像头任务
  xTaskCreatePinnedToCore(
      cameraTask,
      "cameraTask",
      4096, // 堆栈大小（需较大内存）
      &cameraTaskHandle,
      1, // 高优先级
      NULL,
      0 // Core 0
  );
  // Core 1绑定显示任务
  xTaskCreatePinnedToCore(
      displayTask,
      "DisplayTask",
      4096,
      NULL,
      1, // 较低优先级
      NULL,
      1 // Core 1
  );
}

void loop()
{
}
