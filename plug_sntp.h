#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include "led_ctrl.h"

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";
const long gmtOffset_sec = 28800;  // tw need +8hr,28800=8*60*60
const int daylightOffset_sec = 0;  // tw no need daylight save

struct tm timeinfo;

// Store date and time
String current_date;
String current_time;

// Store hour, minute, second
static int32_t hour;
static int32_t minute;
static int32_t second;
static int day_of_week;



bool requestLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    //led_update();  //update_led(LED_R);
    return false;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;
  day_of_week = timeinfo.tm_wday;

  //Serial.printf("timeinfo.tm_year: %d\n", timeinfo.tm_year);
  current_time = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  current_date = String(timeinfo.tm_year + 1900) + "-" + String(timeinfo.tm_mon + 1) + "-" + String(timeinfo.tm_mday);
  //led_update();  //update_led(LED_B_ON);
  //do_lvgl_update = true;

  //Serial.print("ntp Current Time: " + current_time);
  //Serial.println(", Current Date: " + current_date);
  return true;
}

bool get_date_and_time() {

  if (WiFi.status() == WL_CONNECTED) {
    return requestLocalTime();
  } else {
    Serial.println("Not connected to Wi-Fi");
    return false;
  }
}

// Callback function (gets called when time adjusts via NTP)
void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  requestLocalTime();
}

void sntp_setup(void) {
  int retry = 10;

  /**
   * NTP server address could be acquired via DHCP,
   *
   * NOTE: This call should be made BEFORE esp32 acquires IP address via DHCP,
   * otherwise SNTP option 42 would be rejected by default.
   * NOTE: configTime() function call if made AFTER DHCP-client run
   * will OVERRIDE acquired NTP server address
   */
  esp_sntp_servermode_dhcp(1);  // (optional)

  // set notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);

  /**
   * This will set configured ntp servers and constant TimeZone/daylightOffset
   * should be OK if your time zone does not need to adjust daylightOffset twice a year,
   * in such a case time adjustment won't be handled automagically.
   */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

  // Get the time and date from WorldTimeAPI

  while (hour == 0 && minute == 0 && second == 0 && retry) {
    get_date_and_time();
    //led_update();  //update_led(LED_R);
    delay(200);
    //update_led(LED_OFF);
    Serial.print("#");
    retry--;
  }
  // Serial.print("\n");
  // led_update();  //update_led(LED_G);
}
