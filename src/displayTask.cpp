#include "displayTask.h"
#include "cameraTask.h"
#include <TJpg_Decoder.h>

TFT_eSPI tft;
TFT_eSprite sprite = TFT_eSprite(&tft); // 双缓冲 Sprite
int btMIDstate = 0;
static int frames = 0;
float fps = 0;
static unsigned long lastMillis = 0;
bool photoViewMode = false;
SPIClass SPI_LCD(HSPI);
camera_fb_t *fb = NULL;

void displayTask(void *pvParameters)
{
    static bool executed = false;

    while (1)
    {
        if (btMIDstate == 0)
        {
            tft.setSwapBytes(false);
            executed = false;
            photoViewMode = false;
            displaycamera(); // 预览模式

            // if (btTOPstate == 1)
            // {
            //     btTOPstate = 0;
            //     if (special > 5)
            //     {
            //         special = 0;
            //     }
            //     else
            //     {
            //         special++;
            //     }
            //     cameraTask_InitCameraSoftwareConfig();
            // }
            // if (btDownstate == 1)
            // {
            //     btDownstate = 0;
            //     if (special2 > 1)
            //     {
            //         special2 = -2;
            //     }
            //     else
            //     {
            //         special2++;
            //     }
            //     cameraTask_InitCameraSoftwareConfig();
            // }
        }
        else
        {
            // if (!executed)
            // {
            //     // 第一次进入相册
            //     int lastIndex = tfCard_GetNextPhotoIndex() - 1;
            //     if (lastIndex >= 1)
            //     {
            //         currentPhotoIndex = lastIndex;
            //         displayTask_Gallery(currentPhotoIndex);
            //         photoViewMode = true;
            //     }
            //     else
            //     {

            //         displayTask_ErrorLOG("  No photos found .\n\n  Please take photos first .");
            //     }
            //     executed = true;
            // }

            // // 在照片浏览模式中响应 上下键
            // if (photoViewMode)
            // {
            //     if (btTOPstate == 1)
            //     {
            //         btTOPstate = 0;
            //         if (currentPhotoIndex > 1)
            //         {
            //             currentPhotoIndex--;
            //             displayTask_Gallery(currentPhotoIndex);
            //         }
            //     }
            //     if (btDownstate == 1)
            //     {
            //         btDownstate = 0;
            //         if (currentPhotoIndex < tfCard_GetNextPhotoIndex() - 1)
            //         {
            //             currentPhotoIndex++;
            //             displayTask_Gallery(currentPhotoIndex);
            //         }
            //     }
            // }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // 控制刷新速率
    }
}

void displayTask_Init()
{
    SPI_LCD.begin(TFT_SCLK, TFT_MOSI, TFT_CS);
    tft.begin();

    tft.setRotation(1);
    // pinMode(BOARD_LCD_BL, OUTPUT);
    // digitalWrite(BOARD_LCD_BL, HIGH);
    tft.fillScreen(TFT_BLACK);
    // tft.pushImage(0, 0, 320, 240, logo);
    delay(5000);
    tft.fillScreen(TFT_BLACK);
    TJpgDec.setJpgScale(8);
    TJpgDec.setCallback(display_tft_output);
    tft.drawString("Loading......",10,10);
    // tft.setSwapBytes(true); // We need to swap the colour bytes (endianess)
}

void displaycamera()
{
    // ESP_LOGI(TAG, "displaycamera......");
    // 从帧队列中接收摄像头帧缓冲区
    if (xQueueReceive(frameQueue, &fb, portMAX_DELAY) == pdTRUE)
    {
        frames++;
        unsigned long now = millis();
        // 计算 FPS
        if (now - lastMillis >= 1000)
        {
            fps = frames * 1000.0f / (now - lastMillis);
            frames = 0;
            lastMillis = now;
        }

        // 获取图像数据和尺寸
        uint16_t *img = (uint16_t *)fb->buf;
        int w = fb->width;
        int h = fb->height;

        // 创建 Sprite 并显示图像
        sprite.createSprite(w, h);
        sprite.setSwapBytes(false);
        sprite.pushImage(0, 0, w, h, img);

        // 网格绘制
        // displayTask_DrawGrid3x3((uint16_t *)sprite.getPointer(), w, h, TFT_WHITE);

        // 显示 FPS
        sprite.setTextColor(TFT_WHITE);
        sprite.setTextSize(1);
        char infoStr3[32];
        sprintf(infoStr3, "FPS: %d", (int)fps);
        sprite.drawString(infoStr3, 5, 20);

        // 显示固定信息
        // sprite.setTextColor(TFT_YELLOW);
        // sprintf(infoStr3, "Mode:%d", special);
        // sprite.drawString(infoStr3, 210, 15);
        // 显示固定信息
        // sprite.pushImage(290, 105, 30, 30, photo);
        // sprite.pushImage(290, 5, 30, 30, color);
        // sprite.pushImage(0, 0, 150, 25, minilogo);
        // sprite.pushImage(290, 205, 30, 30, sun);
        // 显示 DPI 信息
        // sprite.setTextColor(TFT_YELLOW);
        // sprintf(infoStr3, "light:%d", special2);
        // sprite.drawString(infoStr3, 195, 220);
        // sprintf(infoStr3, "DPI: %dx%d", w, h);
        // sprite.drawString(infoStr3, 5, 220);

        // 弹窗提示
        // if (showSavingPopup)
        // {
        //     sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        //     sprite.drawString(" S A V E ... ", 90, 100);
        // }

        sprite.pushSprite(0, 0);
        sprite.deleteSprite();
        esp_camera_fb_return(fb);
    }
}

bool display_tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    if (y >= tft.height())
        return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}
