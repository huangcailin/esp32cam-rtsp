#ifndef CAM2640_H_
#define CAM2640_H_

#include <Arduino.h>
#include <pgmspace.h>
#include <stdio.h>
#include "OV2640.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_camera.h"

extern camera_config_t esp32cam_config, esp32cam_aithinker_config, esp32cam_ttgo_t_config;

class CAM2640: public OV2640
{
public:
    CAM2640()
    {
        m_fb = NULL;
    };
    ~CAM2640(){
    };
    void run(void);
    size_t getSize(void);
    uint8_t *getfb(void);
    camera_fb_t *getfbObj(void);
    int getWidth(void);
    int getHeight(void);

private:
    void runIfNeeded(); // grab a frame if we don't already have one

    // camera_framesize_t _frame_size;
    // camera_pixelformat_t _pixel_format;
    camera_config_t _cam_config;

    camera_fb_t *m_fb;
};

#endif //CAM2640_H_
