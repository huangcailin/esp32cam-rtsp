#include <Arduino.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include <esp_task_wdt.h>
#include "Common.h"

CamManager g_CamManager;

void Cam_main(void *Param)
{
  CamManager* pCamManager = (CamManager*)Param;
#if ESP_CAMERA_SUPPORTED
  if (ESP_OK != g_CamManager.init_camera())
  {
    ESP_LOGE(TAG, "init camera fail");
    return;
  }
  unsigned long lastSecond = 0;
  unsigned int nFrame = 0;
  unsigned int nCurFrame = 0;
  
  while (1)
  {
    pCamManager->CatchCamera(nFrame, lastSecond);
    vTaskDelay(1);
  }
#else
  ESP_LOGE(TAG, "Camera support is not available for this chip");
  return;
#endif
}

void taskMain(void *Param)
{
  CamManager* pCamManager = (CamManager*)Param;
  // pCamManager->initJpgDec();
  int lastCalib = 0;
  while (1)
  {
    pCamManager->MainLoop();
    vTaskDelay(10 / portTICK_PERIOD_MS); // 防止任务阻塞
  }
}

void setup()
{
  ESP_LOGI(TAG, "setup start......");
  pinMode(FLASH_LIGHT_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  
  digitalWrite(FLASH_LIGHT_PIN, LOW);

  enableCore0WDT();
  enableCore1WDT();
  esp_task_wdt_init(30, true);
  g_CamManager.init_double_buffer();
  g_CamManager.initaialize_tft();
  g_CamManager.initJpgDec();
  // Core 0绑定摄像头任务
  xTaskCreatePinnedToCore(
      taskMain,
      "Main",
      8192, // 堆栈大小（需较大内存）
      &g_CamManager,
      2, // 高优先级
      NULL,
      0 // Core 0
  );
  // Core 1绑定显示任务
  xTaskCreatePinnedToCore(
      Cam_main,
      "Camera",
      4096,
      &g_CamManager,
      1, // 较低优先级
      NULL,
      1 // Core 1
  );
}

void loop()
{
  // ESP_LOGI(TAG, "setup loop......");
  delay(1000);
}
