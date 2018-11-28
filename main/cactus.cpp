 /*
  * LED-Kaktus Ansteuerungssoftware zur Demonstration von
  * Industrieautomatisationsprotokollen.
  *
  * 15.10.2018 O. Rutsch, Sympatec GmbH
  */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <driver/gpio.h>
#include <math.h>
#include "esp32_digital_led_lib.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define HIGH 1
#define LOW 0
#define OUTPUT GPIO_MODE_OUTPUT
#define INPUT GPIO_MODE_INPUT

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))
#define floor(a)   ((int)(a))
#define ceil(a)    ((int)((int)(a) < (a) ? (a+1) : (a)))

void app_cpp_main();

strand_t STRANDS[] = { // Avoid using any of the strapping pins on the ESP32
  {.rmtChannel = 0, .gpioNum = 17, .ledType = LED_WS2813_V2, .brightLimit = 255, .numPixels =  144,
   .pixels = NULL, ._stateVars = NULL},
};

int STRANDCNT = sizeof(STRANDS)/sizeof(STRANDS[0]);

void gpioSetup(int gpioNum, int gpioMode, int gpioVal) {
  #if defined(ARDUINO) && ARDUINO >= 100
    pinMode (gpioNum, gpioMode);
    digitalWrite (gpioNum, gpioVal);
  #elif defined(ESP_PLATFORM)
    gpio_num_t gpioNumNative = (gpio_num_t)(gpioNum);
    gpio_mode_t gpioModeNative = (gpio_mode_t)(gpioMode);
    gpio_pad_select_gpio(gpioNumNative);
    gpio_set_direction(gpioNumNative, gpioModeNative);
    gpio_set_level(gpioNumNative, gpioVal);
  #endif
}

uint32_t IRAM_ATTR millis()
{
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint32_t ms)
{
  if (ms > 0) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
  }
}

void rainbow(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  const uint8_t color_div = 4;
  const uint8_t anim_step = 1;
  const uint8_t anim_max = pStrand->brightLimit - anim_step;
  pixelColor_t color1 = pixelFromRGB(anim_max, 0, 0);
  pixelColor_t color2 = pixelFromRGB(anim_max, 0, 0);
  uint8_t stepVal1 = 0;
  uint8_t stepVal2 = 0;
  bool runForever = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (runForever || (millis() - start_ms < timeout_ms))
  {
    color1 = color2;
    stepVal1 = stepVal2;
    for (uint16_t i = 0; i < pStrand->numPixels; i++)
    {
      pStrand->pixels[i] = pixelFromRGB(color1.r/color_div, color1.g/color_div, color1.b/color_div);
      if (i == 1)
      {
        color2 = color1;
        stepVal2 = stepVal1;
      }
      switch (stepVal1)
      {
        case 0:
        color1.g += anim_step;
        if (color1.g >= anim_max)
          stepVal1++;
        break;
        case 1:
        color1.r -= anim_step;
        if (color1.r == 0)
          stepVal1++;
        break;
        case 2:
        color1.b += anim_step;
        if (color1.b >= anim_max)
          stepVal1++;
        break;
        case 3:
        color1.g -= anim_step;
        if (color1.g == 0)
          stepVal1++;
        break;
        case 4:
        color1.r += anim_step;
        if (color1.r >= anim_max)
          stepVal1++;
        break;
        case 5:
        color1.b -= anim_step;
        if (color1.b == 0)
          stepVal1 = 0;
        break;
      }
    }
    digitalLeds_updatePixels(pStrand);
    delay(delay_ms);
  }
  digitalLeds_resetPixels(pStrand);
}

class Rainbower
{
  private:
    const uint8_t color_div = 4;
    const uint8_t anim_step = 5;
    uint8_t anim_max;
    uint8_t stepVal1;
    uint8_t stepVal2;
    uint8_t StepCounter = 0;
    pixelColor_t color1;
    pixelColor_t color2;
  public:
    Rainbower();
    pixelColor_t drawNext();
};

Rainbower::Rainbower()
{
  anim_max = 255 - anim_step;
  stepVal1 = 0;
  stepVal2 = 0;
  color1 = pixelFromRGB(anim_max, 0, 0);
  color2 = pixelFromRGB(anim_max, 0, 0);
}

pixelColor_t Rainbower::drawNext()
{
  //color1 = color2;
  //stepVal1 = stepVal2;

  pixelColor_t CurrentColor= pixelFromRGB(color1.r/color_div, color1.g/color_div, color1.b/color_div);
/*
  if (StepCounter == 1)
  {
    color2 = color1;
    stepVal2 = stepVal1;
  }*/
  switch (stepVal1)
  {
    case 0:
    color1.g += anim_step;
    if (color1.g >= anim_max)
      stepVal1++;
    break;
    case 1:
    color1.r -= anim_step;
    if (color1.r == 0)
      stepVal1++;
    break;
    case 2:
    color1.b += anim_step;
    if (color1.b >= anim_max)
      stepVal1++;
    break;
    case 3:
    color1.g -= anim_step;
    if (color1.g == 0)
      stepVal1++;
    break;
    case 4:
    color1.r += anim_step;
    if (color1.r >= anim_max)
      stepVal1++;
    break;
    case 5:
    color1.b -= anim_step;
    if (color1.b == 0)
      stepVal1 = 0;
    break;
  }
  StepCounter++;
  return CurrentColor;
}


class Scannerer
{
  private:
    strand_t * pStrand;
    pixelColor_t BlackColor;
    unsigned int currIdx;
  public:
    Scannerer(strand_t *);
    void drawNext(pixelColor_t);
};

Scannerer::Scannerer(strand_t * pStrandIn)
{
  pStrand = pStrandIn;
  BlackColor = pixelFromRGB(0, 0, 0);
  currIdx = 0;
}

void Scannerer::drawNext(pixelColor_t aColor)
{
  const int GlowTail=30;
  float r,g,b;
  for (int i=0;i<GlowTail;i++)
  {
    pixelColor_t CurrentColor=aColor;
    r=CurrentColor.r;
    g=CurrentColor.g;
    b=CurrentColor.b;
    r/=((20.0/GlowTail)*i+1);
    g/=((20.0/GlowTail)*i+1);
    b/=((20.0/GlowTail)*i+1);
    CurrentColor.r=r;
    CurrentColor.g=g;
    CurrentColor.b=b;
    pStrand->pixels[(currIdx-i) % pStrand->numPixels] = CurrentColor;
  }
  pStrand->pixels[(currIdx-GlowTail) % pStrand->numPixels] = BlackColor;
  digitalLeds_updatePixels(pStrand);
  currIdx++;
}

Rainbower MyRainbow;

void scanner(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  Scannerer MyScanner(pStrand);

  for (int i = 0; i < pStrand->numPixels; i++)
  {
    pixelColor_t c=MyRainbow.drawNext();
    MyScanner.drawNext(c);
    delay(delay_ms);
  }
  //digitalLeds_resetPixels(pStrand);
}

extern "C" int app_main(void)
{
  app_cpp_main();
  return 0;
}

void app_cpp_main()
{
  gpioSetup(17, OUTPUT, LOW);
  gpioSetup(2, OUTPUT, HIGH);

  if (digitalLeds_initStrands(STRANDS, STRANDCNT)) {
    ets_printf("Init FAILURE: halting\n");
    while (true) {};
  }
  bool toggle=true;
  Rainbower MyRainbow;
  strand_t * pStrand = &STRANDS[0];
  Scannerer MyScanner(pStrand);
  while (true)
  {
    pixelColor_t c=MyRainbow.drawNext();
    MyScanner.drawNext(c);
    delay(20);

    toggle=!toggle;
    gpio_set_level(GPIO_NUM_2,(uint32_t)toggle);
  }
  vTaskDelete(NULL);
}
