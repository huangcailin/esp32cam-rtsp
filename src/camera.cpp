#include "camera.h"


void CAM2640::run(void)
{
    if(m_fb)
        //return the frame buffer back to the driver for reuse
        esp_camera_fb_return(m_fb);

    m_fb = esp_camera_fb_get();
}

void CAM2640::runIfNeeded(void)
{
    if(!m_fb)
        run();
}

int CAM2640::getWidth(void)
{
    runIfNeeded();
    return m_fb->width;
}

int CAM2640::getHeight(void)
{
    runIfNeeded();
    return m_fb->height;
}

size_t CAM2640::getSize(void)
{
    runIfNeeded();
    return m_fb->len;
}

uint8_t *CAM2640::getfb(void)
{
    runIfNeeded();
    return m_fb->buf;
}

camera_fb_t *CAM2640::getfbObj(void)
{
    return m_fb;
}