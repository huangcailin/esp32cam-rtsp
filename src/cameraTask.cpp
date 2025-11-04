#include "cameraTask.h"
#include "config.h"

int special2 = 0;
int special = 0;
QueueHandle_t frameQueue; // 声明队列，用于存储预览画面帧数据

esp_err_t cameraTask_Init()
{
    // 确保cam断开
    esp_err_t err = esp_camera_deinit(); // 摄像头去初始化   防止没断电导致cam初始化失败
    ESP_LOGE(TAG, "摄像头去初始化: 0x%x\n", err);
    // pinMode(32, OUTPUT); //  32是  cam电源控制引脚
    // digitalWrite(32, 1); //  高电平  断电 cam的
    // delay(20);
    // pinMode(32, INPUT); // 释放  引脚
    // initialize the camera
    err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        delay(2000);
        while (1)
        {
            ESP_LOGE(TAG, "摄像头初始化失败，错误代码: 0x%x\n", err);
            // 如果初始化失败，等待100毫秒后重试
            ESP_LOGE(TAG, "Not Found Camera.\n\nPlease check the camera connection and reboot.");
            esp_camera_deinit();
            if (esp_camera_init(&camera_config) == ESP_OK)
            {
                tft.fillScreen(TFT_BLACK);
                break; // 成功则跳出循环
            }
            delay(2000);
        }
    }
    // 创建帧队列
    frameQueue = xQueueCreate(1, sizeof(camera_fb_t *));
    if (!frameQueue)
    {
        ESP_LOGE(TAG, "Failed to create frame queue");
    }
    return ESP_OK;
}

void cameraTask(void *pvParameters)
{
    while (1)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        if (xQueueSend(frameQueue, &fb, 0) != pdTRUE)
        {
            esp_camera_fb_return(fb);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void cameraTask_InitCameraSoftwareConfig()
{
    sensor = esp_camera_sensor_get();
    /* 传感器默认配置 */

    sensor->set_contrast(sensor, special2);   // 对比度：0 (中间值，范围通常-2~2)
    sensor->set_brightness(sensor, special2); // 亮度：0 (中间值，范围通常-2~2)
    sensor->set_saturation(sensor, special2); // 饱和度：0 (中间值，范围通常-2~2)
    // sensor->set_sharpness(sensor, 1);                   // 锐度：0 (关闭锐化)
    // sensor->set_denoise(sensor, 1);                   // 降噪：0 (关闭降噪)
    // sensor->set_gainceiling(sensor, GAINCEILING_8X);      // 增益上限：8倍 (防止过曝)
    // sensor->set_quality(sensor, 10);                  // JPEG质量：10 (0-63，值越低压缩率越高)
    // sensor->set_colorbar(sensor, 0);                   // 彩条测试：0 (关闭测试模式)
    // sensor->set_whitebal(sensor, 1);                   // 自动白平衡：1 (开启)
    // sensor->set_gain_ctrl(sensor, 1);                   // 自动增益控制：1 (开启)
    // sensor->set_exposure_ctrl(sensor, 1);                   // 自动曝光控制：1 (开启)
    // sensor->set_hmirror(sensor, 0);                   // 水平镜像：0 (关闭)
    #if defined(OV2640)
        sensor->set_vflip(sensor, 0);    // 1280x1024
    #elif defined(OV5640)
        sensor->set_vflip(sensor, 1);     // 2560x1920
    #else
        sensor->set_vflip(sensor, 1);    // 1600x1200 (安全默认值)
    #endif
    // 垂直翻转：0 (关闭)
    // sensor->set_aec2(sensor, 1);                   // AEC2算法：0 (关闭)
    // sensor->set_awb_gain(sensor, 1);                   // AWB增益：1 (开启)
    // sensor->set_agc_gain(sensor, 0);                  // AGC增益值：0 (自动)
    // sensor->set_aec_value(sensor, 1200);                 // 曝光值：600 (1-1200，单位行数)
    sensor->set_special_effect(sensor, special); // 特效：0 (无特效)
    // sensor->set_wb_mode(sensor, 0);                   // 白平衡模式：0 (自动)
    // sensor->set_ae_level(sensor, 1);                   // AE补偿：0 (无补偿)
    // sensor->set_dcw(sensor, 1);                   // DCW(下采样)：1 (开启)
    // sensor->set_bpc(sensor, 1);                   // 坏点校正：1 (开启)
    // sensor->set_wpc(sensor, 1);                   // 白点校正：1 (开启)
    // sensor->set_raw_gma(sensor, 1);                   // RAW伽马校正：1 (开启)
    // sensor->set_lenc(sensor, 1);                   // 镜头校正：1 (开启)
    // 时钟频率：20MHz (典型工作频率)
}
