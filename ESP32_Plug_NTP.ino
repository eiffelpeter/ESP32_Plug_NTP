#include <Wire.h>         // i2c
#include <Preferences.h>  // esp32 nvs
//#include <WiFi.h>
//#include "time.h"
#include "OTAWebUpdater.h"
#include "plug_sntp.h"
#include "led_ctrl.h"
#include "plug_timer.h"
#include "esp_mac.h"// For esp_read_mac

const char compile_date[] = __DATE__ " " __TIME__;

/* I2C */
#define I2C_SCL 23
#define I2C_SDA 18
#define I2C_FREQ 400000

// define the GPIO connected with Relays and Buttons
#define RelayPin 19
#define Button 35
#define wifiLed 25
#define pl7211_rst 22

// nvs
Preferences preferences;

volatile bool button_change = false;
static unsigned long check_wifi_tick, button_tick, ota_service_tick;
bool wifi_connected;
bool request_sync_time = false;

void check_relay_on_off(void) {
  if (relay_off_time != relay_on_time) {
    if (relay_on_time > relay_off_time) {
      if ((hour >= relay_off_time) && (hour < relay_on_time)) {  // off
        if (digitalRead(RelayPin) == HIGH)
          onSwitch1Change(0);
      } else {  // on
        if (digitalRead(RelayPin) == LOW)
          onSwitch1Change(1);
      }
    } else {
      if ((hour >= relay_on_time) && (hour < relay_off_time)) {  // on
        if (digitalRead(RelayPin) == LOW)
          onSwitch1Change(1);
      } else {  // off
        if (digitalRead(RelayPin) == HIGH)
          onSwitch1Change(0);
      }
    }
  }
}

static void loop_second_refresh(void) {
  second++;
  if (second > 59) {
    second = 0;
    minute++;

    //Serial.printf("time: %d:%d:%d \n", hour, minute, second);

    if (minute > 59) {
      minute = 0;
      hour++;
      request_sync_time = true;
      Serial.println("request to sync time");

      if (hour > 23) {
        hour = 0;
      }

      // relay
      //check_relay_on_off();
      if (hour == relay_off_time) {
        onSwitch1Change(0);
      }

      if (hour == relay_on_time) {
        onSwitch1Change(1);
      }
    }
  }
}

void led_update(void) {
  if (switch1 == 1) {
    wifi_connected ? set_status_led(LED_B_FADE) : set_status_led(LED_B_BLINK);
  } else {
    wifi_connected ? set_status_led(LED_W_FADE) : set_status_led(LED_W_BLINK);
  }
}

void onSwitch1Change(int on) {
  switch1 = on;

  //Control the device
  if (on == 1) {
    digitalWrite(RelayPin, HIGH);
    Serial.print("Device1 ON");
  } else {
    digitalWrite(RelayPin, LOW);
    Serial.print("Device1 OFF");
  }

  Serial.printf(" at time: %d:%d:%d \n", hour, minute, second);
  if (on != preferences.getInt("switch1", 1)) {
    preferences.putInt("switch1", on);
  }
  led_update();
}

void button_isr() {
  volatile static int button_status = HIGH;
  if (digitalRead(Button) == LOW && (button_status == HIGH)) {
    button_tick = millis();
    button_status = LOW;
  } else if ((digitalRead(Button) == HIGH) && (button_status == LOW)) {
    button_tick = millis() - button_tick;
    button_status = HIGH;
    button_change = true;
  }
}

void check_nvs(void) {
  if (save_nvs) {

    switch (save_nvs) {
      case 1:
        preferences.putInt("relay_off_time", relay_off_time);
        preferences.putInt("relay_on_time", relay_on_time);
        check_relay_on_off();
        break;
      case 2:
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        delay(1000);
        ESP.restart();  // restart to connect new wifi ap
        break;
    }
    save_nvs = 0;
  }
}

void setup() {
  int retry = 50;

  // Initialize serial and wait for port to open:
  Serial.begin(115200);
  for (unsigned long const serialBeginTime = millis(); !Serial && (millis() - serialBeginTime <= 5000);) {}

  pinMode(Button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Button), button_isr, CHANGE);

  pinMode(RelayPin, OUTPUT);
  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, HIGH);  //Turn off WiFi LED

  led_init();
  digitalRead(RelayPin) ? set_status_led(LED_B_BLINK) : set_status_led(LED_W_BLINK);

  // nvs
  preferences.begin("my-plug", false);
  relay_off_time = preferences.getInt("relay_off_time", relay_off_time);
  relay_on_time = preferences.getInt("relay_on_time", relay_on_time);
  ssid = preferences.getString("ssid", ssid);
  password = preferences.getString("password", password);
  Serial.printf("ssid: %s \n", ssid);
  Serial.printf("password: %s \n", password);
  onSwitch1Change(preferences.getInt("switch1", 1));

  // Initialize the I2C bus
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  Serial.println("Initializing I2C bus...");

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP); // Read the Bluetooth MAC address
  char deviceName[30];
  sprintf(deviceName, "PLUG_%02X%02X%02X%02X%02X%02X",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Set WiFi mode to AP_STA
  WiFi.mode(WIFI_AP_STA);

  // Configure SoftAP
  WiFi.softAP(deviceName, "12345678");  // SSID, Password
  Serial.print("SoftAP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  set_status_led(LED_B_BLINK);
  while ((WiFi.status() != WL_CONNECTED) && retry) {
    Serial.print(".");
    delay(200);
    retry--;
  }
  Serial.print("\n");

  if (retry) {
    Serial.print("\nConnected to Wi-Fi network with IP Address: ");
    Serial.println(WiFi.localIP());
    wifi_connected = true;
  } else {
    wifi_connected = false;
  }

  /* init OTA web */
  webServerInit();

  // sntp
  sntp_setup();

  // plug timer
  plug_timer_setup();

  Serial.print("build at:");
  Serial.println(compile_date);

  check_wifi_tick = millis();
  led_update();
}

void loop() {

  // If Timer has fired, every 1s
  if (plug_timer_expire() == true) {
    loop_second_refresh();

    if (request_sync_time) {
      request_sync_time = get_date_and_time() ? false : true;
    }
  }

  if (button_change) {
    button_change = false;
    if (button_tick > 2000) {
      // long press
      Serial.println("clear all nvs, reboot");
      preferences.clear();
      delay(1000);
      ESP.restart();
    } else {
      onSwitch1Change(digitalRead(RelayPin) ? 0 : 1);
    }
  }

  led_refresh();

  check_nvs();

  // ota
  if (millis() - ota_service_tick > 150) {
    ota_service_tick = millis();
    server.handleClient();
  }

  // reconnect every 10 min
  if (millis() - check_wifi_tick > 600000) {
    check_wifi_tick = millis();
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
    } else {
      wifi_connected = false;
      WiFi.begin(ssid, password);
      Serial.println("try reconnect wifi");
    }
    led_update();
  }
}