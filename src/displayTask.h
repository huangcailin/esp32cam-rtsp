#include <Arduino.h>
#include <TFT_eSPI.h>
#pragma once
extern SPIClass SPI_LCD; 
extern QueueHandle_t frameQueue;

void displayTask(void *pvParameters);
void displayTask_Init();
void displaycamera();
bool display_tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);