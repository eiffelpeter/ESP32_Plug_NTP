#ifndef LED_CTRL_H_
#define LED_CTRL_H_

#include <Arduino.h>

typedef enum {
  LED_B_ON,
  LED_W_ON,
  LED_B_FADE,
  LED_W_FADE,
  LED_B_BLINK,
  LED_W_BLINK,
  LED_OFF,
} RGBW_LED;

void set_status_led(RGBW_LED led);
RGBW_LED get_status_led();
void led_refresh();
void led_init();

#endif