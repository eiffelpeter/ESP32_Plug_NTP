#include <Preferences.h>  // esp32 nvs
#include "OTAWebUpdater.h"
#include "plug_sntp.h"
#include "plug_timer.h"
#include "esp_mac.h"  // For esp_read_mac

const char compile_date[] = __DATE__ " " __TIME__;

#define RELAY_CTRL_INV 1

// DOIT ESP32 DEVKIT
// define the GPIO connected with Relays and Buttons
#define RelayPin 19
#define Button 0
#define wifiLed 2

// nvs
Preferences preferences;

volatile bool button_change = false;
static unsigned long check_wifi_tick, button_tick, ota_service_tick;
bool wifi_connected;
bool request_sync_time = false;
int relay_current_time;

void wifiLed_toggle(void) {
  digitalWrite(wifiLed, digitalRead(wifiLed) ? LOW : HIGH);
}

void check_relay_on_off(void) {

  if (relay_off_time != relay_on_time) {
    if (relay_on_time > relay_off_time) {
      if ((relay_current_time >= relay_off_time) && (relay_current_time < relay_on_time)) {  // off
        if (1 == switch1)
          onSwitch1Change(0);
      } else {  // on
        if (0 == switch1)
          onSwitch1Change(1);
      }
    } else {
      if ((relay_current_time >= relay_on_time) && (relay_current_time < relay_off_time)) {  // on
        if (0 == switch1)
          onSwitch1Change(1);
      } else {  // off
        if (1 == switch1)
          onSwitch1Change(0);
      }
    }
  } else
    Serial.println("warning on off time is same");
}

static void loop_second_refresh(void) {
  second++;
  if (second > 59) {
    second = 0;
    minute++;

    if (minute > 59) {
      minute = 0;
      hour++;
      request_sync_time = true;
      Serial.println("request to sync time every hour");

      if (hour > 23) {
        hour = 0;
      }
    }


    // check time to on-off relay
    relay_current_time = hour * 60 + minute;

    //Serial.printf("time: %d:%d:%d \n", hour, minute, second);
    //check_relay_on_off();
    if (relay_current_time == relay_off_time) {
      onSwitch1Change(0);
    }

    if (relay_current_time == relay_on_time) {
      onSwitch1Change(1);
    }

  }

  if (wifi_connected == false)
    wifiLed_toggle();
}

void onSwitch1Change(int on) {
  switch1 = on;

  //Control the device
  if (on == 1) {
    digitalWrite(RelayPin, RELAY_CTRL_INV ? LOW : HIGH);
    Serial.print("Device1 ON");
  } else {
    digitalWrite(RelayPin, RELAY_CTRL_INV ? HIGH : LOW);
    Serial.print("Device1 OFF");
  }

  Serial.printf(" at time: %d:%d:%d \n", hour, minute, second);
  if (on != preferences.getInt("switch1", 1)) {
    preferences.putInt("switch1", on);
  }
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
        WiFi.softAPdisconnect();
        delay(1000);
        ESP.restart();  // restart to connect new wifi ap
        break;
      case 3:
        preferences.putString("dev_name", dev_name);
        WiFi.softAPdisconnect();
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
  digitalWrite(wifiLed, LOW);  //Turn off WiFi LED

  switch1 = RELAY_CTRL_INV ? (!digitalRead(RelayPin)) : digitalRead(RelayPin);

  // nvs
  preferences.begin("my-plug", false);
  relay_off_time = preferences.getInt("relay_off_time", relay_off_time);
  relay_on_time = preferences.getInt("relay_on_time", relay_on_time);
  ssid = preferences.getString("ssid", ssid);
  password = preferences.getString("password", password);
  Serial.printf("ssid: %s \n", ssid);
  Serial.printf("password: %s \n", password);
  onSwitch1Change(preferences.getInt("switch1", 1));

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);  // Read the Bluetooth MAC address
  char deviceName[30];
  snprintf(deviceName, sizeof(deviceName), "Plug_%02X%02X%02X", mac[3], mac[4], mac[5]);  // default device name for softAP
  dev_name = String(deviceName);
  dev_name = preferences.getString("dev_name", dev_name);

  // Set WiFi mode to AP_STA
  WiFi.mode(WIFI_AP_STA);

  // Configure SoftAP
  WiFi.softAP(dev_name, "1qaz2wsx");  // SSID, Password
  Serial.print("SoftAP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while ((WiFi.status() != WL_CONNECTED) && retry) {
    wifiLed_toggle();
    Serial.print(".");
    delay(200);
    retry--;
  }
  Serial.print("\n");

  if (retry) {
    Serial.print("\nConnected to Wi-Fi network with IP Address: ");
    Serial.println(WiFi.localIP());
    wifi_connected = true;
    digitalWrite(wifiLed, HIGH);  //Turn on WiFi LED
  } else {
    wifi_connected = false;
    digitalWrite(wifiLed, LOW);  //Turn off WiFi LED
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
      onSwitch1Change(switch1 ? 0 : 1);
    }
  }

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
      digitalWrite(wifiLed, HIGH);  //Turn on WiFi LED
    } else {
      wifi_connected = false;
      WiFi.begin(ssid, password);
      Serial.println("try reconnect wifi");
    }
  }
}