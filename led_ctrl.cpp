#include "led_ctrl.h"

#define blueLed 14
#define whiteLed 27

#define MAX_LED_BRIGHTNESS 64  // 64 steps
#define MIN_LED_BRIGHTNESS 0   //

RGBW_LED status_led;
int fade_step, duty, keep_fade_min;
unsigned long led_interval;

void set_status_led(RGBW_LED led) {
  status_led = led;
  duty = MIN_LED_BRIGHTNESS;
  fade_step = 1;
  keep_fade_min = 0;

  ledcWrite(blueLed, MIN_LED_BRIGHTNESS);   // off
  ledcWrite(whiteLed, MIN_LED_BRIGHTNESS);  // off

  switch (led) {
    case LED_B_ON:
      ledcWrite(blueLed, MAX_LED_BRIGHTNESS);  // on
      break;
    case LED_W_ON:
      ledcWrite(whiteLed, MAX_LED_BRIGHTNESS);  // on
      break;
    case LED_B_FADE:
      led_interval = 80;  // fade cycle = 80ms * 64 steps * 2 = 10.24sec
      break;
    case LED_W_FADE:
      led_interval = 80;
      break;
    case LED_B_BLINK:
      led_interval = 500;
      break;
    case LED_W_BLINK:
      led_interval = 500;
      break;
    case LED_OFF:
    default:
      break;
  }
}

RGBW_LED get_status_led() {
  return status_led;
}

void led_refresh() {
  static unsigned long led_tick;
  uint8_t pin;

  if (millis() - led_tick > led_interval) {
    led_tick = millis();

    switch (status_led) {
      case LED_B_FADE:
        pin = blueLed;
        goto FADE;
        break;
      case LED_W_FADE:
        pin = whiteLed;
        goto FADE;
        break;
      case LED_B_BLINK:
        pin = blueLed;
        goto BLINK;
        break;
      case LED_W_BLINK:
        pin = whiteLed;
        goto BLINK;
        break;
      default:
        return;
        break;
    }

FADE:
    duty += fade_step;

    if ((duty < MIN_LED_BRIGHTNESS) || (duty > MAX_LED_BRIGHTNESS))
      log_e("invalid pwm duty %d", duty);

    ledcWrite(pin, duty);

    if ((duty == MIN_LED_BRIGHTNESS) && keep_fade_min) {
      fade_step = 0;
      keep_fade_min--;
    } else if (duty <= MIN_LED_BRIGHTNESS) {
      fade_step = 1;
    } else if (duty >= MAX_LED_BRIGHTNESS) {
      fade_step = -1;
      keep_fade_min = 25;  // 25 * 80ms = 2 sec
    }
    return;

BLINK:
    duty = (duty == MIN_LED_BRIGHTNESS) ? MAX_LED_BRIGHTNESS : MIN_LED_BRIGHTNESS;
    ledcWrite(pin, duty);
    return;
  }
}

void led_init() {
  ledcAttach(blueLed, 1000, 8);  // 1 kHz PWM, 8-bit resolution
  ledcOutputInvert(blueLed, true);
  ledcWrite(blueLed, MIN_LED_BRIGHTNESS);   // off
  delay(1);
  ledcAttach(whiteLed, 12000, 8);
  ledcOutputInvert(whiteLed, true);  
  ledcWrite(whiteLed, MIN_LED_BRIGHTNESS);  // off  
  delay(1);
}