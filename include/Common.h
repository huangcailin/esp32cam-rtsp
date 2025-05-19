#ifndef CAM_COMMON_H
#define CAM_COMMON_H

#include <EEPROM.h>
#include <esp_log.h>
#include <TFT_eSPI.h>
#include <SD_MMC.h>
#include <FS.h>
#include <TJpg_Decoder.h>
#include "esp_jpg_decode.h"
#include "esp_camera.h"
#include "config.h"
#include "menu.h"

static const char *TAG = "MY_CAM";
// JPEG 数据结构
struct JPEGData {
    const uint8_t* data;
    size_t size;
    size_t position;
};

// RGB565 颜色转换宏
#define RGB888_TO_RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

struct Settings
{
    int iFlag;
    int iPictureIndex;
    bool bUseFlashLight;
};

typedef struct
{
  uint8_t *producerBuffer;
  uint8_t *consumerBuffer;
  uint8_t *pProducerBuf;
  uint8_t *pConsumerBuf;
  size_t nPic_Len;
  size_t nPic_H;
  size_t nPic_W;
  size_t nFrame;
  SemaphoreHandle_t switch_mutex;
} double_buffer_t;

enum CtrlMode
{
    CtrlMode_None = 0,
    CtrlMode_MENU,
    CtrlMode_CAMERE
};

struct StatusInfo
{
    StatusInfo()
    {
        oCurMode = CtrlMode_CAMERE;
        oPreMode = CtrlMode_None;
    }
    CtrlMode oCurMode;
    CtrlMode oPreMode;
};

class EEPROM_Manager
{
private:
    /* data */
public:
    EEPROM_Manager(/* args */);
    ~EEPROM_Manager();
    void InitConfig();
    void ClearAll();
    void SetUseFlashLight(bool bUse);
    bool IsUseFlashFlight();
    void UpdatePicIndex(int nIndex);
    int GetPicIndex();
    void SaveAllConfig();

private:
    Settings m_mConfig;
};

class CamManager

{
private:
    /* data */
public:
    CamManager(/* args */);
    ~CamManager();

    void close_tft();
    bool GetJpeg(size_t &jpeg_size, uint8_t **jpeg_buf);
    void initJpgDec();
    void initaialize_tft();
    void init_double_buffer();
    void TakePhoto();
    esp_err_t init_camera();
    void OnButtonClick();
    void CatchCamera(unsigned int nCurFrame);
    void MainLoop();
    void DrawMsg();
    void ShowMsg(string strMsg);

private:
    void SavePhoto(uint8_t *img_buf, size_t iLen);
    static bool StaticJpegRender(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    bool jpegRender(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    void ShowScaledJPG(double_buffer_t& oDataBuff);

private:
    TFT_eSPI m_tft = TFT_eSPI();
    CMyMenu mMenu;
    double_buffer_t m_oDataBuff;
    string m_strMsg;
    unsigned long m_MsgKeepTime;
    QueueHandle_t m_frameQueue;

public:
    static CamManager* m_instance; // 保存当前对象实例
    EEPROM_Manager m_EEPROM_Manager;
    StatusInfo m_Status;
};

int adc_power_key();

#endif