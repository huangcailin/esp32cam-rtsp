#include <Arduino.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <driver/i2c.h>
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <OV2640.h>
#include <ESPmDNS.h>
#include <rtsp_server.h>
#include <lookup_camera_effect.h>
#include <lookup_camera_frame_size.h>
#include <lookup_camera_gainceiling.h>
#include <lookup_camera_wb_mode.h>
#include <format_duration.h>
#include <format_number.h>
#include <moustache.h>
#include <settings.h>
#include <SD_MMC.h>
#include <NTPClient.h>
#include <TFT_eSPI.h>
#include "camera.h"

// HTML files
extern const char index_html_min_start[] asm("_binary_html_index_min_html_start");

auto param_group_camera = iotwebconf::ParameterGroup("camera", "Camera settings");
auto param_frame_duration = iotwebconf::Builder<iotwebconf::UIntTParameter<unsigned long>>("fd").label("Frame duration (ms)").defaultValue(DEFAULT_FRAME_DURATION).min(10).build();
auto param_frame_size = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(frame_sizes[0])>>("fs").label("Frame size").optionValues((const char *)&frame_sizes).optionNames((const char *)&frame_sizes).optionCount(sizeof(frame_sizes) / sizeof(frame_sizes[0])).nameLength(sizeof(frame_sizes[0])).defaultValue(DEFAULT_FRAME_SIZE).build();
auto param_jpg_quality = iotwebconf::Builder<iotwebconf::UIntTParameter<byte>>("q").label("JPG quality").defaultValue(DEFAULT_JPEG_QUALITY).min(1).max(100).build();
auto param_brightness = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("b").label("Brightness").defaultValue(DEFAULT_BRIGHTNESS).min(-2).max(2).build();
auto param_contrast = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("c").label("Contrast").defaultValue(DEFAULT_CONTRAST).min(-2).max(2).build();
auto param_saturation = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("s").label("Saturation").defaultValue(DEFAULT_SATURATION).min(-2).max(2).build();
auto param_special_effect = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_effects[0])>>("e").label("Effect").optionValues((const char *)&camera_effects).optionNames((const char *)&camera_effects).optionCount(sizeof(camera_effects) / sizeof(camera_effects[0])).nameLength(sizeof(camera_effects[0])).defaultValue(DEFAULT_EFFECT).build();
auto param_whitebal = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wb").label("White balance").defaultValue(DEFAULT_WHITE_BALANCE).build();
auto param_flash_light_bal = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("fl").label("Flash light").defaultValue(DEFAULT_FLASH_LIGHT).build();
auto param_awb_gain = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("awbg").label("Automatic white balance gain").defaultValue(DEFAULT_WHITE_BALANCE_GAIN).build();
auto param_wb_mode = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_wb_modes[0])>>("wbm").label("White balance mode").optionValues((const char *)&camera_wb_modes).optionNames((const char *)&camera_wb_modes).optionCount(sizeof(camera_wb_modes) / sizeof(camera_wb_modes[0])).nameLength(sizeof(camera_wb_modes[0])).defaultValue(DEFAULT_WHITE_BALANCE_MODE).build();
auto param_exposure_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("ec").label("Exposure control").defaultValue(DEFAULT_EXPOSURE_CONTROL).build();
auto param_aec2 = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("aec2").label("Auto exposure (dsp)").defaultValue(DEFAULT_AEC2).build();
auto param_ae_level = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("ael").label("Auto Exposure level").defaultValue(DEFAULT_AE_LEVEL).min(-2).max(2).build();
auto param_aec_value = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("aecv").label("Manual exposure value").defaultValue(DEFAULT_AEC_VALUE).min(9).max(1200).build();
auto param_gain_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("gc").label("Gain control").defaultValue(DEFAULT_GAIN_CONTROL).build();
auto param_agc_gain = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("agcg").label("AGC gain").defaultValue(DEFAULT_AGC_GAIN).min(0).max(30).build();
auto param_gain_ceiling = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_gain_ceilings[0])>>("gcl").label("Auto Gain ceiling").optionValues((const char *)&camera_gain_ceilings).optionNames((const char *)&camera_gain_ceilings).optionCount(sizeof(camera_gain_ceilings) / sizeof(camera_gain_ceilings[0])).nameLength(sizeof(camera_gain_ceilings[0])).defaultValue(DEFAULT_GAIN_CEILING).build();
auto param_bpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("bpc").label("Black pixel correct").defaultValue(DEFAULT_BPC).build();
auto param_wpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wpc").label("White pixel correct").defaultValue(DEFAULT_WPC).build();
auto param_raw_gma = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("rg").label("Gamma correct").defaultValue(DEFAULT_RAW_GAMMA).build();
auto param_lenc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("lenc").label("Lens correction").defaultValue(DEFAULT_LENC).build();
auto param_hmirror = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("hm").label("Horizontal mirror").defaultValue(DEFAULT_HORIZONTAL_MIRROR).build();
auto param_vflip = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("vm").label("Vertical mirror").defaultValue(DEFAULT_VERTICAL_MIRROR).build();
auto param_dcw = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("dcw").label("Downsize enable").defaultValue(DEFAULT_DCW).build();
auto param_colorbar = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("cb").label("Colorbar").defaultValue(DEFAULT_COLORBAR).build();

// Camera
CAM2640 cam;
// DNS Server
DNSServer dnsServer;
// RTSP Server
std::unique_ptr<rtsp_server> camera_server;
// Web server
WebServer web_server(80);

auto thingName = String(WIFI_SSID) + "-" + String(ESP.getEfuseMac(), 16);
IotWebConf iotWebConf(thingName.c_str(), &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

// Camera initialization result
esp_err_t camera_init_result;
// 共享队列（用于双核通信）
QueueHandle_t imageQueue = xQueueCreate(1, sizeof(void*));
TFT_eSPI tft = TFT_eSPI();

// NTP 配置
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
// 函数定义
String getDateTime();
void SavePhoto(uint8_t * img_buf, size_t iLen);
bool GetJpeg(size_t& jpeg_size, uint8_t ** jpeg_buf);

void handle_root()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  // Format hostname
  auto hostname = "esp32-" + WiFi.macAddress() + ".local";
  hostname.replace(":", "");
  hostname.toLowerCase();

  // Wifi Modes
  const char *wifi_modes[] = {"NULL", "STA", "AP", "STA+AP"};
  auto ipv4 = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIP() : WiFi.localIP();
  auto ipv6 = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIPv6() : WiFi.localIPv6();

  auto initResult = esp_err_to_name(camera_init_result);
  if (initResult == nullptr)
    initResult = "Unknown reason";

  moustache_variable_t substitutions[] = {
      // Version / CPU
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"BoardType", BOARD_NAME},
      {"ThingName", iotWebConf.getThingName()},
      {"SDKVersion", ESP.getSdkVersion()},
      {"ChipModel", ESP.getChipModel()},
      {"ChipRevision", String(ESP.getChipRevision())},
      {"CpuFreqMHz", String(ESP.getCpuFreqMHz())},
      {"CpuCores", String(ESP.getChipCores())},
      {"FlashSize", format_memory(ESP.getFlashChipSize(), 0)},
      {"HeapSize", format_memory(ESP.getHeapSize())},
      {"PsRamSize", format_memory(ESP.getPsramSize(), 0)},
      // Diagnostics
      {"Uptime", String(format_duration(millis() / 1000))},
      {"FreeHeap", format_memory(ESP.getFreeHeap())},
      {"MaxAllocHeap", format_memory(ESP.getMaxAllocHeap())},
      {"NumRTSPSessions", camera_server != nullptr ? String(camera_server->num_connected()) : "RTSP server disabled"},
      // Network
      {"HostName", hostname},
      {"MacAddress", WiFi.macAddress()},
      {"AccessPoint", WiFi.SSID()},
      {"SignalStrength", String(WiFi.RSSI())},
      {"WifiMode", wifi_modes[WiFi.getMode()]},
      {"IPv4", ipv4.toString()},
      {"IPv6", ipv6.toString()},
      {"NetworkState.ApMode", String(iotWebConf.getState() == iotwebconf::NetworkState::ApMode)},
      {"NetworkState.OnLine", String(iotWebConf.getState() == iotwebconf::NetworkState::OnLine)},
      // Camera
      {"FrameSize", String(param_frame_size.value())},
      {"FrameDuration", String(param_frame_duration.value())},
      {"FrameFrequency", String(1000.0 / param_frame_duration.value(), 1)},
      {"JpegQuality", String(param_jpg_quality.value())},
      {"CameraInitialized", String(camera_init_result == ESP_OK)},
      {"CameraInitResult", String(camera_init_result)},
      {"CameraInitResultText", initResult},
      // Settings
      {"Brightness", String(param_brightness.value())},
      {"Contrast", String(param_contrast.value())},
      {"Saturation", String(param_saturation.value())},
      {"SpecialEffect", String(param_special_effect.value())},
      {"FlashLight", String(param_flash_light_bal.value())},
      {"WhiteBal", String(param_whitebal.value())},
      {"AwbGain", String(param_awb_gain.value())},
      {"WbMode", String(param_wb_mode.value())},
      {"ExposureCtrl", String(param_exposure_ctrl.value())},
      {"Aec2", String(param_aec2.value())},
      {"AeLevel", String(param_ae_level.value())},
      {"AecValue", String(param_aec_value.value())},
      {"GainCtrl", String(param_gain_ctrl.value())},
      {"AgcGain", String(param_agc_gain.value())},
      {"GainCeiling", String(param_gain_ceiling.value())},
      {"Bpc", String(param_bpc.value())},
      {"Wpc", String(param_wpc.value())},
      {"RawGma", String(param_raw_gma.value())},
      {"Lenc", String(param_lenc.value())},
      {"HMirror", String(param_hmirror.value())},
      {"VFlip", String(param_vflip.value())},
      {"Dcw", String(param_dcw.value())},
      {"ColorBar", String(param_colorbar.value())},
      // RTSP
      {"RtspPort", String(RTSP_PORT)}};

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  auto html = moustache_render(index_html_min_start, substitutions);
  web_server.send(200, "text/html", html);
}

void handle_snapshot()
{
  log_v("handle_snapshot");
  if (camera_init_result != ESP_OK)
  {
    web_server.send(404, "text/plain", "Camera is not initialized");
    return;
  }
  if(bool(param_flash_light_bal.value()))
  {
    SD_MMC.end();
    digitalWrite(FLASH_LIGHT_PIN, HIGH);
  }
  // Remove old images stored in the frame buffer
  auto frame_buffers = CAMERA_CONFIG_FB_COUNT;
  while (frame_buffers--)
    cam.run();

  size_t fb_len = 0;
  uint8_t *pJpeg_buf = nullptr;
  bool bRet = GetJpeg(fb_len, &pJpeg_buf);
  if(bool(param_flash_light_bal.value()))
    digitalWrite(FLASH_LIGHT_PIN, LOW);
  if (!bRet)
  {
    String strContent = "Unable to obtain frame buffer from the camera:";
    strContent +=((bRet==true)?"true":"false");
    web_server.send(404, "text/plain", strContent);
    return;
  }
  
  // SavePhoto(pJpeg_buf, fb_len);
  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  web_server.setContentLength(fb_len);
  web_server.send(200, "image/jpeg", "");
  web_server.sendContent((const char*)pJpeg_buf, fb_len);
  delete pJpeg_buf;
  delay(500);
  esp_restart();
}

#define STREAM_CONTENT_BOUNDARY "123456789000000000000987654321"

void handle_stream()
{
  log_v("handle_stream");
  if (camera_init_result != ESP_OK)
  {
    web_server.send(404, "text/plain", "Camera is not initialized");
    return;
  }

  log_v("starting streaming");
  // Blocks further handling of HTTP server until stopped
  char size_buf[12];
  auto client = web_server.client();
  client.write("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: multipart/x-mixed-replace; boundary=" STREAM_CONTENT_BOUNDARY "\r\n");
  while (client.connected())
  {
    client.write("\r\n--" STREAM_CONTENT_BOUNDARY "\r\n");
    cam.run();
    client.write("Content-Type: image/jpeg\r\nContent-Length: ");
    sprintf(size_buf, "%d\r\n\r\n", cam.getSize());
    client.write(size_buf);
    client.write(cam.getfb(), cam.getSize());
  }

  log_v("client disconnected");
  client.stop();
  log_v("stopped streaming");
}

void initaialize_tft()
{
  tft.begin();
  tft.setRotation(1); // 设置屏幕方向（0-3）
  tft.fillScreen(TFT_BLACK); // 清屏

  // 显示文本
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Hello, ESP32-CAM!");

  // 显示矩形
  tft.fillRect(50, 50, 60, 60, TFT_RED);
}

esp_err_t initialize_camera()
{
  log_v("initialize_camera");

  log_i("Frame size: %s", param_frame_size.value());
  auto frame_size = lookup_frame_size(param_frame_size.value());
  log_i("JPEG quality: %d", param_jpg_quality.value());
  auto jpeg_quality = param_jpg_quality.value();
  log_i("Frame duration: %d ms", param_frame_duration.value());
  const camera_config_t camera_config = {
    .pin_pwdn = CAMERA_CONFIG_PIN_PWDN,         // GPIO pin for camera power down line
    .pin_reset = CAMERA_CONFIG_PIN_RESET,       // GPIO pin for camera reset line
    .pin_xclk = CAMERA_CONFIG_PIN_XCLK,         // GPIO pin for camera XCLK line
    .pin_sccb_sda = CAMERA_CONFIG_PIN_SCCB_SDA, // GPIO pin for camera SDA line
    .pin_sccb_scl = CAMERA_CONFIG_PIN_SCCB_SCL, // GPIO pin for camera SCL line
    .pin_d7 = CAMERA_CONFIG_PIN_Y9,             // GPIO pin for camera D7 line
    .pin_d6 = CAMERA_CONFIG_PIN_Y8,             // GPIO pin for camera D6 line
    .pin_d5 = CAMERA_CONFIG_PIN_Y7,             // GPIO pin for camera D5 line
    .pin_d4 = CAMERA_CONFIG_PIN_Y6,             // GPIO pin for camera D4 line
    .pin_d3 = CAMERA_CONFIG_PIN_Y5,             // GPIO pin for camera D3 line
    .pin_d2 = CAMERA_CONFIG_PIN_Y4,             // GPIO pin for camera D2 line
    .pin_d1 = CAMERA_CONFIG_PIN_Y3,             // GPIO pin for camera D1 line
    .pin_d0 = CAMERA_CONFIG_PIN_Y2,             // GPIO pin for camera D0 line
    .pin_vsync = CAMERA_CONFIG_PIN_VSYNC,       // GPIO pin for camera VSYNC line
    .pin_href = CAMERA_CONFIG_PIN_HREF,         // GPIO pin for camera HREF line
    .pin_pclk = CAMERA_CONFIG_PIN_PCLK,         // GPIO pin for camera PCLK line
    .xclk_freq_hz = CAMERA_CONFIG_CLK_FREQ_HZ,  // Frequency of XCLK signal, in Hz. EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
    .ledc_timer = CAMERA_CONFIG_LEDC_TIMER,     // LEDC timer to be used for generating XCLK
    .ledc_channel = CAMERA_CONFIG_LEDC_CHANNEL, // LEDC channel to be used for generating XCLK
    .pixel_format = PIXFORMAT_RGB565,             // Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG
    .frame_size = frame_size,                   // Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    .jpeg_quality = jpeg_quality,               // Quality of JPEG output. 0-63 lower means higher quality
    .fb_count = CAMERA_CONFIG_FB_COUNT,         // Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)
    .fb_location = CAMERA_CONFIG_FB_LOCATION,   // The location where the frame buffer will be allocated
    .grab_mode = CAMERA_GRAB_LATEST,            // When buffers should be filled
#if CONFIG_CAMERA_CONVERTER_ENABLED
    conv_mode = CONV_DISABLE, // RGB<->YUV Conversion mode
#endif
    .sccb_i2c_port = SCCB_I2C_PORT // If pin_sccb_sda is -1, use the already configured I2C bus by number
  };
  esp_camera_deinit();
  return cam.init(camera_config);
}

void update_camera_settings()
{
  auto camera = esp_camera_sensor_get();
  if (camera == nullptr)
  {
    log_e("Unable to get camera sensor");
    return;
  }

  camera->set_brightness(camera, param_brightness.value());
  camera->set_contrast(camera, param_contrast.value());
  camera->set_framesize(camera, lookup_frame_size(param_frame_size.value()));
  camera->set_saturation(camera, param_saturation.value());
  camera->set_special_effect(camera, lookup_camera_effect(param_special_effect.value()));
  camera->set_whitebal(camera, param_whitebal.value());
  camera->set_awb_gain(camera, param_awb_gain.value());
  camera->set_wb_mode(camera, lookup_camera_wb_mode(param_wb_mode.value()));
  camera->set_exposure_ctrl(camera, param_exposure_ctrl.value());
  camera->set_aec2(camera, param_aec2.value());
  camera->set_ae_level(camera, param_ae_level.value());
  camera->set_aec_value(camera, param_aec_value.value());
  camera->set_gain_ctrl(camera, param_gain_ctrl.value());
  camera->set_agc_gain(camera, param_agc_gain.value());
  camera->set_gainceiling(camera, lookup_camera_gainceiling(param_gain_ceiling.value()));
  camera->set_bpc(camera, param_bpc.value());
  camera->set_wpc(camera, param_wpc.value());
  camera->set_raw_gma(camera, param_raw_gma.value());
  camera->set_lenc(camera, param_lenc.value());
  camera->set_hmirror(camera, param_hmirror.value());
  camera->set_vflip(camera, param_vflip.value());
  camera->set_dcw(camera, param_dcw.value());
  camera->set_colorbar(camera, param_colorbar.value());
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  camera_server = std::unique_ptr<rtsp_server>(new rtsp_server(cam, param_frame_duration.value(), RTSP_PORT));
  // Add RTSP service to mDNS
  // HTTP is already set by iotWebConf
  MDNS.addService("rtsp", "tcp", RTSP_PORT);
}

void on_connected()
{
  log_v("on_connected");
  // Start the RTSP Server if initialized
  if (camera_init_result == ESP_OK)
    start_rtsp_server();
  else
    log_e("Not starting RTSP server: camera not initialized");
}

void on_config_saved()
{
  log_v("on_config_saved");
  update_camera_settings();
}

// 获取当前日期和时间
String getDateTime()
{
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime(&rawtime);

  char buffer[20];
  sprintf(buffer, "%04d%02d%02d_%02d%02d%02d",
          ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buffer);
}

// 拍照并保存到 SD 卡
void SavePhoto(uint8_t * img_buf, size_t iLen) 
{
  // 检查 SD 卡是否已挂载
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed, trying again...");
    return;
  }
  // 获取当前日期和时间
  String dateTime = getDateTime();
  String folderName = "/" + dateTime.substring(0, 8); // 提取日期作为文件夹名
  String fileName = folderName + "/" + dateTime.substring(9, dateTime.length()) + ".jpg";

  // 创建文件夹
  if (!SD_MMC.exists(folderName.c_str())) {
    SD_MMC.mkdir(folderName.c_str());
  }

  // 保存图片到 SD 卡
  File file = SD_MMC.open(fileName.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.write(img_buf, iLen);
  file.close();
  Serial.println("Photo saved: " + fileName);
}

bool GetJpeg(size_t& jpeg_size, uint8_t** jpeg_buf)
{
  camera_fb_t* m_fb = cam.getfbObj();
  if(m_fb)
    esp_camera_fb_return(m_fb);
  // 将RGB565数据转换为JPEG
  bool jpeg_converted = frame2jpg(m_fb, 80, jpeg_buf, &jpeg_size); // 80是JPEG质量（0-100）
  if (!jpeg_converted) {
    Serial.println("JPEG Conversion Failed");
    esp_camera_fb_return(m_fb);
    return false;
  }
  esp_camera_fb_return(m_fb);
  return true;
}

void taskCamera(void *pvParameters) 
{
  // Try to initialize 3 times
  for (auto i = 0; i < 3; i++)
  {
    camera_init_result = initialize_camera();
    if (camera_init_result == ESP_OK)
    {
      update_camera_settings();
      break;
    }

    esp_camera_deinit();
    log_e("Failed to initialize camera. Error: 0x%0x. Frame size: %s, frame rate: %d ms, jpeg quality: %d", camera_init_result, param_frame_size.value(), param_frame_duration.value(), param_jpg_quality.value());
    delay(500);
  }

  while (1) {
    if (camera_server)
      camera_server->doLoop();
    cam.run();
    void *p=nullptr;
    xQueueSend(imageQueue, &p, portMAX_DELAY); // 发送到队列
    vTaskDelay(10 / portTICK_PERIOD_MS); // 控制采集频率‌:ml-citation{ref="5" data="citationList"}
  }
}

void taskMain(void *pvParameters)
 {
  if (CAMERA_CONFIG_FB_LOCATION == CAMERA_FB_IN_PSRAM && !psramInit())
    log_e("Failed to initialize PSRAM");

  param_group_camera.addItem(&param_frame_duration);
  param_group_camera.addItem(&param_frame_size);
  param_group_camera.addItem(&param_jpg_quality);
  param_group_camera.addItem(&param_brightness);
  param_group_camera.addItem(&param_contrast);
  param_group_camera.addItem(&param_saturation);
  param_group_camera.addItem(&param_special_effect);
  param_group_camera.addItem(&param_flash_light_bal);
  param_group_camera.addItem(&param_whitebal);
  param_group_camera.addItem(&param_awb_gain);
  param_group_camera.addItem(&param_wb_mode);
  param_group_camera.addItem(&param_exposure_ctrl);
  param_group_camera.addItem(&param_aec2);
  param_group_camera.addItem(&param_ae_level);
  param_group_camera.addItem(&param_aec_value);
  param_group_camera.addItem(&param_gain_ctrl);
  param_group_camera.addItem(&param_agc_gain);
  param_group_camera.addItem(&param_gain_ceiling);
  param_group_camera.addItem(&param_bpc);
  param_group_camera.addItem(&param_wpc);
  param_group_camera.addItem(&param_raw_gma);
  param_group_camera.addItem(&param_lenc);
  param_group_camera.addItem(&param_hmirror);
  param_group_camera.addItem(&param_vflip);
  param_group_camera.addItem(&param_dcw);
  param_group_camera.addItem(&param_colorbar);
  iotWebConf.addParameterGroup(&param_group_camera);

  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setConfigSavedCallback(on_config_saved);
  iotWebConf.setWifiConnectionCallback(on_connected);
#ifdef USER_LED_GPIO
  iotWebConf.setStatusPin(USER_LED_GPIO, USER_LED_ON_LEVEL);
#endif
  iotWebConf.init();

  
  initaialize_tft();

  // // 初始化 SD 卡
  // if (!SD_MMC.begin("/sdcard", true)) {
  //   Serial.println("SD Card Mount Failed");
  // }
  // else
  //   Serial.println("SD Card Mounted");

  // Set up required URL handlers on the web server
  web_server.on("/", HTTP_GET, handle_root);
  web_server.on("/config", []{ iotWebConf.handleConfig(); });
  // Camera snapshot
  web_server.on("/snapshot", HTTP_GET, handle_snapshot);
  // Camera stream
  web_server.on("/stream", HTTP_GET, handle_stream);

  web_server.onNotFound([](){ iotWebConf.handleNotFound(); });

  while (1)
  {
    char *pTmp;
    if (xQueueReceive(imageQueue, &pTmp, portMAX_DELAY)) 
    {
      tft.pushImage(0, 0, cam.getWidth(), cam.getHeight(), (uint16_t *)cam.getfb());
      
      // 显示文本
      // tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(10, 10);
      // tft.println("hello @");
      tft.println(cam.getWidth());
      tft.setCursor(10, 20);
      tft.println(cam.getHeight());
      
    }
    iotWebConf.doLoop();
    vTaskDelay(1); // 防止任务阻塞‌:ml-citation{ref="2,7" data="citationList"}
  }
}


void setup()
{
  // Disable brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

#ifdef USER_LED_GPIO
  pinMode(USER_LED_GPIO, OUTPUT);
  digitalWrite(USER_LED_GPIO, !USER_LED_ON_LEVEL);
#endif

  pinMode(FLASH_LIGHT_PIN, OUTPUT);
  digitalWrite(FLASH_LIGHT_PIN, LOW);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

#ifdef ARDUINO_USB_CDC_ON_BOOT
  // Delay for USB to connect/settle
  delay(5000);
#endif

  log_i("Core debug level: %d", CORE_DEBUG_LEVEL);
  log_i("CPU Freq: %d Mhz, %d core(s)", getCpuFrequencyMhz(), ESP.getChipCores());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("SDK version: %s", ESP.getSdkVersion());
  log_i("Board: %s", BOARD_NAME);
  log_i("Starting " APP_TITLE "...");

  // Core 0绑定摄像头任务
  xTaskCreatePinnedToCore(
    taskCamera, 
    "Camera", 
    8192,  // 堆栈大小（需较大内存）
    NULL, 
    2,     // 高优先级
    NULL, 
    0      // Core 0
  );

  // Core 1绑定显示任务
  xTaskCreatePinnedToCore(
    taskMain, 
    "Display", 
    4096, 
    NULL, 
    1,     // 较低优先级
    NULL, 
    1      // Core 1
  );
  
}

void loop()
{
}