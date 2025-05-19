#include "Common.h"

EEPROM_Manager ::EEPROM_Manager(/* args */)
{
    m_mConfig.bUseFlashLight = false;
    InitConfig();
}

EEPROM_Manager ::~EEPROM_Manager()
{
}

void EEPROM_Manager::InitConfig()
{
    EEPROM.get(0, m_mConfig);
    if (m_mConfig.iFlag != 1)
    {
        ClearAll();
        m_mConfig.iFlag = 1;
        SaveAllConfig();
    }
}

void EEPROM_Manager::ClearAll()
{
    // 清空EEPROM（全部写0）
    for (int i = 0; i < EEPROM.length(); i++)
    {
        EEPROM.write(i, 0);
    }
}

void EEPROM_Manager::SetUseFlashLight(bool bUse)
{
    m_mConfig.bUseFlashLight = bUse;
    SaveAllConfig();
}

bool EEPROM_Manager::IsUseFlashFlight()
{
    return true; // m_mConfig.bUseFlashLight;
}

void EEPROM_Manager::UpdatePicIndex(int nIndex)
{
    m_mConfig.iPictureIndex = nIndex;
    SaveAllConfig();
}

int EEPROM_Manager::GetPicIndex()
{
    UpdatePicIndex(++m_mConfig.iPictureIndex);
    return m_mConfig.iPictureIndex;
}

void EEPROM_Manager::SaveAllConfig()
{
    EEPROM.put(0, m_mConfig);
}

int adc_power_key()
{                                         // 1-4为对应按键短按，5-8为按键长按， 0是可更新电源电压，//-1是在判断是否长按短按
    static unsigned int adc_flag_num = 0; // 计算adc判断 非0连续次数，用于判断按键长按 短按逻辑
    static unsigned int adc_key_num = 0;  // 上次的按键数  用于长按 短按 的判断逻辑
    static unsigned int adc_pre_value = 0;
    static unsigned int adc_ignore_times = 4; // 按钮触发时忽略前4次的浮动值

    int adc_num = analogRead(BUTTON_PIN);
    if (adc_num > 1900)
    {
        if (adc_flag_num == 0)
            return 0; // 无按键按下  此时为检测电池电压
        else
        {
            if (adc_pre_value < 400)
            { // 按键4检测   拍照/取消
                adc_key_num = 4;
            }
            else if (adc_pre_value < 800)
            { // 按键3   设置/切换模式
                adc_key_num = 3;
            }
            else if (adc_pre_value < 1300)
            { // 按键2
                adc_key_num = 2;
            }
            else
            { // 按键1
                adc_key_num = 1;
            }

            if (adc_flag_num > 30)
            { // 长按
                adc_flag_num = 0;
                adc_pre_value = 0;
                return adc_key_num + 4;
            }
            else if (adc_flag_num <= 15)
            { // 短按
                adc_flag_num = 0;
                adc_pre_value = 0;
                return adc_key_num;
            }
            else
            {
                adc_flag_num = 0; // 在这之间，无响应
                adc_pre_value = 0;
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "[%d]adc_pre_value=%d", adc_num, adc_pre_value);
        adc_flag_num++;
        if (adc_flag_num >= adc_ignore_times)
        {
            adc_pre_value = _max(adc_pre_value, adc_num);
        }
    }

    return -1; // 按键按下，还在判断长按还是短按的时候
}
CamManager *CamManager::m_instance = nullptr;
CamManager::CamManager(/* args */) : mMenu(&m_tft)
{
    m_instance = this;
    // m_oDataBuff.switch_mutex = xSemaphoreCreateMutex();
}

CamManager::~CamManager()
{
}

void CamManager::close_tft()
{ // 关闭tft屏及背光   SD卡使用时无效
    m_tft.deInitDMA();
    delay(100);
    pinMode(TFT_RST, OUTPUT);   //  tft的复位引脚  也连着BLK背光控制引脚
    digitalWrite(TFT_RST, LOW); // 低电平为关闭
}

void CamManager::SavePhoto(uint8_t *img_buf, size_t iLen)
{
    m_tft.deInitDMA();

    pinMode(TFT_RST, INPUT_PULLUP);
    pinMode(TFT_DC, INPUT_PULLUP);
    pinMode(TFT_MOSI, INPUT_PULLUP);

    delay(100);
    // 检查 SD 卡是否已挂载
    if (!SD_MMC.begin("/sdcard", true))
    {
        ESP_LOGE(TAG, "SD Card Mount Failed, trying again...");
        ShowMsg("SD Card Error");
        return;
    }

    String folderName = "/Photos";
    String fileName = folderName + "/" + m_EEPROM_Manager.GetPicIndex() + ".jpg";
    // 创建文件夹
    if (!SD_MMC.exists(folderName.c_str()))
    {
        SD_MMC.mkdir(folderName.c_str());
    }

    while (SD_MMC.exists(fileName))
        fileName = folderName + "/" + m_EEPROM_Manager.GetPicIndex() + ".jpg";

    // 保存图片到 SD 卡
    fs::File file = SD_MMC.open(fileName.c_str(), FILE_WRITE);
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    file.write(img_buf, iLen);
    file.close();
    ESP_LOGI(TAG, "Photo saved: %s", fileName.c_str());
    SD_MMC.end();
    delay(100);
    esp_restart();
}

bool CamManager::GetJpeg(size_t &jpeg_size, uint8_t **jpeg_buf)
{
    if (m_EEPROM_Manager.IsUseFlashFlight())
    {
        digitalWrite(FLASH_LIGHT_PIN, HIGH);
    }
    camera_fb_t *m_fb = esp_camera_fb_get();
    if (!m_fb)
    {
        if (m_EEPROM_Manager.IsUseFlashFlight())
            digitalWrite(FLASH_LIGHT_PIN, LOW);
        return false;
    }
    esp_camera_fb_return(m_fb);

    if (m_EEPROM_Manager.IsUseFlashFlight())
        digitalWrite(FLASH_LIGHT_PIN, LOW);
    // 将RGB565数据转换为JPEG
    bool jpeg_converted = frame2jpg(m_fb, 80, jpeg_buf, &jpeg_size); // 80是JPEG质量（0-100）
    if (!jpeg_converted)
    {
        ESP_LOGE(TAG, "JPEG Conversion Failed");
        return false;
    }
    return true;
}

void CamManager::initJpgDec()
{
    ESP_LOGI(TAG, "initJpgDec");
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(&CamManager::StaticJpegRender);
    ESP_LOGI(TAG, "initJpgDec succ");
}

void CamManager::initaialize_tft()
{
    ESP_LOGI(TAG, "Init screen");

    pinMode(TFT_RST, OUTPUT);  // TFT DC
    pinMode(TFT_DC, OUTPUT);   // TFT MOSI
    pinMode(TFT_MOSI, OUTPUT); // TFT SCK

    m_tft.initDMA();
    m_tft.begin();
    m_tft.setRotation(1);        // 设置屏幕方向（0-3）
    m_tft.fillScreen(TFT_BLACK); // 清屏

    // 显示文本
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    m_tft.setTextSize(2);
    m_tft.setCursor(10, 10);
    m_tft.println("MINI-CAM");

    // 显示矩形
    m_tft.fillRect(50, 50, 60, 60, TFT_GREENYELLOW);

    ESP_LOGI(TAG, "Init screen end");
}

void CamManager::init_double_buffer()
{
    ESP_LOGI(TAG, "Init buffer");
    m_oDataBuff.producerBuffer = (uint8_t *)heap_caps_malloc(IMAGE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    m_oDataBuff.pProducerBuf = m_oDataBuff.producerBuffer;
    m_oDataBuff.consumerBuffer = (uint8_t *)heap_caps_malloc(IMAGE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    m_oDataBuff.pConsumerBuf = m_oDataBuff.consumerBuffer;
    m_frameQueue = xQueueCreate(1, sizeof(uint8_t*));
    m_oDataBuff.switch_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Init buffer end");
}

void CamManager::TakePhoto()
{
    size_t fb_len = 0;
    uint8_t *pJpeg_buf = nullptr;
    bool bRet = GetJpeg(fb_len, &pJpeg_buf);
    if (bRet)
        SavePhoto(pJpeg_buf, fb_len);
}

esp_err_t CamManager::init_camera()
{
    // init_double_buffer();
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void CamManager::OnButtonClick()
{
    int adc_flag = adc_power_key();
    if (m_Status.oCurMode == CtrlMode_CAMERE)
    {
        switch (adc_flag)
        {
        // case -1:     break;
        case 0:
            break; // ESP_LOGI(TAG, "");  //电池电压获取成功
        case 1:
            ESP_LOGI(TAG, "CAMERE key1 Short press");
            TakePhoto();
            break; // 按键1短按
        case 2:
            ESP_LOGI(TAG, "CAMERE key2 Short press");
            break; // 按键2短按
        case 3:
            ESP_LOGI(TAG, "CAMERE key3 Short press");
            break; // 按键3短按
        case 4:
            ESP_LOGI(TAG, "CAMERE key4 Short press");
            m_Status.oCurMode = CtrlMode_MENU;
            mMenu.SetNeedRefresh();
            mMenu.DrawMainMenu();
            ESP_LOGI(TAG, "CurMod:%d", m_Status.oCurMode);
            break; // 按键4短按
        case 5:
            ESP_LOGI(TAG, "CAMERE key1 Long press");
            esp_restart();
            break; // 按键1长按
        case 6:
            ESP_LOGI(TAG, "CAMERE key2 Long press");
            break; // 按键2长按
        case 7:
            ESP_LOGI(TAG, "CAMERE key3 Long press");
            break; // 按键3长按
        case 8:
            ESP_LOGI(TAG, "CAMERE key4 Long press");
            break; // 按键4长按
        default:
            break;
        }
    }
    else if (m_Status.oCurMode == CtrlMode_MENU)
    {
        switch (adc_flag)
        {
        // case -1:     break;
        case 0:
            break; // ESP_LOGI(TAG, "");  //电池电压获取成功
        case 1:
            ESP_LOGI(TAG, "MENU key1 Short press");
            mMenu.onDownClick();
            break; // 按键1短按
        case 2:
            ESP_LOGI(TAG, "MENU key2 Short press");
            mMenu.onEnterClick();
            break; // 按键2短按
        case 3:
            ESP_LOGI(TAG, "MENUkey3 Short press");
            mMenu.OnUpClick();
            break; // 按键3短按
        case 4:
            ESP_LOGI(TAG, "MENU key4 Short press");
            if (!mMenu.OnMenuClick())
                m_Status.oCurMode = CtrlMode_CAMERE;
            break; // 按键4短按
        case 5:
            ESP_LOGI(TAG, "MENU key1 Long press");
            break; // 按键1长按
        case 6:
            ESP_LOGI(TAG, "MENU key2 Long press");
            break; // 按键2长按
        case 7:
            ESP_LOGI(TAG, "MENU key3 Long press");
            break; // 按键3长按
        case 8:
            ESP_LOGI(TAG, "MENU key4 Long press");
            break; // 按键4长按
        default:
            break;
        }
    }
}

void CamManager::CatchCamera(unsigned int nCurFrame)
{
    if (m_Status.oCurMode != CtrlMode_CAMERE)
    {
        vTaskDelay(5);
        return;
    }
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic)
    {
        ESP_LOGI(TAG, "CatchCamera err ");
        vTaskDelay(10 / portTICK_PERIOD_MS); // 获取失败时延时重试
        return;
    }
    if (xSemaphoreTake(m_oDataBuff.switch_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // ESP_LOGI(TAG, "CatchCamera:%d", pic->len);
        // TJpgDec.setJpgScale(1.0f * TFT_WIDTH / pic->width); // 计算缩放比例
        // JRESULT jresult = TJpgDec.drawJpg(0, 0, (const uint8_t *)pic->buf, (uint32_t)pic->len);
        if (pic->len <= IMAGE_BUF_SIZE)
        {
            memcpy((void *)m_oDataBuff.pProducerBuf, pic->buf, pic->len);
            // 交换缓冲区指针
            uint8_t *temp = m_oDataBuff.pProducerBuf;
            m_oDataBuff.pProducerBuf = m_oDataBuff.pConsumerBuf;
            m_oDataBuff.pConsumerBuf = temp;
        }
        // ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        m_oDataBuff.nPic_W = pic->width;
        m_oDataBuff.nPic_H = pic->height;
        m_oDataBuff.nPic_Len = pic->len;
        // int nPic_Len = pic->len;
        m_oDataBuff.nFrame = nCurFrame;
        // vTaskDelay(10 / portTICK_PERIOD_MS); 
        xSemaphoreGive(m_oDataBuff.switch_mutex);
        // 发送通知到队列
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(m_frameQueue, &m_oDataBuff.pConsumerBuf, &xHigherPriorityTaskWoken);
        if(xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
    esp_camera_fb_return(pic);
}

void CamManager::MainLoop()
{
    OnButtonClick();
    // ESP_LOGI(TAG, "taskMain CurMod:%d", m_Status.oCurMode);
    if (m_Status.oCurMode == CtrlMode_CAMERE)
    {
        uint8_t *frameToDisplay = NULL;
        if(xQueueReceive(m_frameQueue, &frameToDisplay, pdMS_TO_TICKS(3)))
        {
            // uint32_t start = millis();
            ShowScaledJPG(m_oDataBuff);
            // m_tft.pushImage(0, 0, m_oDataBuff.nPic_W, m_oDataBuff.nPic_H, (uint16_t *)m_oDataBuff.pConsumerBuf);
            // ESP_LOGI(TAG, "MainLoop:%d", millis()-start);
            if (m_Status.oCurMode != m_Status.oPreMode)
            {
                m_Status.oPreMode = m_Status.oCurMode;
                m_tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
                m_tft.setTextSize(1);
            }

            m_tft.setCursor(10, 10);
            m_tft.println(m_oDataBuff.nPic_W);
            m_tft.setCursor(10, 20);
            m_tft.println(m_oDataBuff.nPic_H);
            m_tft.setCursor(10, 30);
            m_tft.println(m_oDataBuff.nFrame);
        }
    }
    else if (m_Status.oCurMode == CtrlMode_MENU)
    {
        if (m_Status.oCurMode != m_Status.oPreMode)
        {
            m_Status.oPreMode = m_Status.oCurMode;
        }
    }
    DrawMsg();
}

void CamManager::DrawMsg()
{
    if (millis() - m_MsgKeepTime < 3000 && !m_strMsg.empty())
    {
        m_tft.setCursor(20, m_tft.height() / 2);
        m_tft.println(m_strMsg.c_str());
    }
    else
    {
        m_MsgKeepTime = millis();
    }
}

void CamManager::ShowMsg(string strMsg)
{
    m_strMsg = strMsg;
    m_MsgKeepTime = millis();
}

bool CamManager::jpegRender(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // m_tft.startWrite();
    m_tft.pushImage(x, y, w, h, bitmap);
    // m_tft.endWrite();
    return true;
}

bool CamManager::StaticJpegRender(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    if (!m_instance)
        return false;
    return m_instance->jpegRender(x, y, w, h, bitmap);
}

// JPEG缩放显示函数
void CamManager::ShowScaledJPG(double_buffer_t &oDataBuff)
{
    // uint32_t start = millis();
    uint16_t w, h;

    m_tft.startWrite();
    TJpgDec.setJpgScale(1.0f * TFT_WIDTH / oDataBuff.nPic_W); // 计算缩放比例
    TJpgDec.drawJpg(0, 0, (const uint8_t *)oDataBuff.pConsumerBuf, (uint32_t)oDataBuff.nPic_Len);

    m_tft.endWrite();
    // Serial.printf("Display time: %dms\n", millis()-start);
}