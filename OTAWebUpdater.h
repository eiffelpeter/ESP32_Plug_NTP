#ifndef OTAWEBUPDATER_H_
#define OTAWEBUPDATER_H_

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Ticker.h>

#include "html.h"
#include "led_ctrl.h"

WebServer server(80);
Ticker tkSecond;
uint8_t otaDone = 0;

int switch1;
int relay_off_time = 19;
int relay_on_time = 8;
String ssid = "openwrt-2g";
String password = "12345678";
int save_nvs = 0;

void onSwitch1Change(int on);

void handleRoot() {

  String html;

  // relay control on web
  html += "<form>";
  if (switch1) {
    html += "<input type=\"button\" value=\"OFF\" onclick=\"location.href='/toggle'\">";
    html += "<br>";
    html += "Now relay is on";
  } else {
    html += "<input type=\"button\" value=\"ON\" onclick=\"location.href='/toggle'\">";
    html += "<br>";
    html += "Now relay is off";
  }
  html += "<br>";
  html += "=====================================";
  html += "</form>";

  // relay on off time
  html += "<form action=\"/submitNumber\" method=\"get\">";
  html += "relay off time hour(0~23): <input type=\"number\" name=\"offNumber\" value=\"" + String(relay_off_time) + "\">";
  html += "<br>";
  html += "relay on time hour(0~23): <input type=\"number\" name=\"onNumber\" value=\"" + String(relay_on_time) + "\">";
  html += "<br>";
  html += "<input type=\"submit\" value=\"Submit\">";
  html += "<br>";
  html += "=====================================";
  html += "</form>";

  // ssid and password
  html += "<form action=\"/submitSSID\" method=\"get\">";
  html += "ssid: <input type=\"string\" name=\"ssidStr\" value=\"" + ssid + "\">";
  html += "<br>";
  html += "password: <input type=\"string\" name=\"passwdStr\" >";
  html += "<br>";
  html += "<input type=\"submit\" value=\"Enter\">";
  html += "<br>";
  html += "=====================================";
  html += "</form>";

  // ota update
  html += indexHtml;  // include ota select bin button

  server.send(200, "text/html", html);
}

void handleNumberSubmission() {
  int tmp1, tmp2;

  if ((server.hasArg("offNumber")) && (server.hasArg("onNumber"))) {
    tmp1 = server.arg("offNumber").toInt();
    tmp2 = server.arg("onNumber").toInt();

    server.sendHeader("Refresh", "10");
    server.sendHeader("Location", "/");
    server.send(307);

    // check value are valid ?
    if (((tmp1 >= 0) && (tmp1 <= 23)) && ((tmp2 >= 0) && (tmp2 <= 23))) {
      relay_off_time = tmp1;
      relay_on_time = tmp2;
      save_nvs = 1;
    }

    Serial.printf("received time hour for relay off: %d, relay on: %d \n", relay_off_time, relay_on_time);
  } else {
    server.send(400, "text/plain", "No number provided.");
  }
}

void handleSSIDSubmission() {
  String tmp1, tmp2;

  if ((server.hasArg("ssidStr")) && (server.hasArg("passwdStr"))) {
    tmp1 = server.arg("ssidStr");
    tmp2 = server.arg("passwdStr");

    server.sendHeader("Refresh", "10");
    server.sendHeader("Location", "/");
    server.send(307);

    // check value are valid ?
    if ((tmp1 != "") && (tmp2 != "")) {
      ssid = tmp1;
      password = tmp2;
      save_nvs = 2;
    }

    Serial.printf("received ssid: %s, passwd: %s \n", ssid, password);
  } else {
    server.send(400, "text/plain", "No number provided.");
  }
}

void handleToggle() {
  onSwitch1Change(switch1 ? 0 : 1);

  //digitalWrite(ledPin, !digitalRead(ledPin));  // Toggle LED
  //server.sendHeader("Location", "/");          // Redirect back to homepage
  //server.send(303);                            // 303 See Other
  server.sendHeader("Refresh", "10");
  server.sendHeader("Location", "/");
  server.send(307);

  Serial.printf("handleToggle relay is %s \n", switch1 ? "on" : "off");
}

void everySecond() {
  if (otaDone > 1) {
    (get_status_led() == LED_OFF) ? set_status_led(LED_B_ON) : set_status_led(LED_OFF);  // Blink
    Serial.printf("ota: %d%%\n", otaDone);
  }
}

void handleUpdateEnd() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(502, "text/plain", Update.errorString());
  } else {
    server.sendHeader("Refresh", "10");
    server.sendHeader("Location", "/");
    server.send(307);
    delay(1000);
    ESP.restart();
  }
}

void handleUpdate() {
  size_t fsize = UPDATE_SIZE_UNKNOWN;
  if (server.hasArg("size")) {
    fsize = server.arg("size").toInt();
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Receiving Update: %s, Size: %d\n", upload.filename.c_str(), fsize);
    if (!Update.begin(fsize)) {
      otaDone = 0;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    } else {
      otaDone = 100 * Update.progress() / Update.size();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
    } else {
      Serial.printf("%s\n", Update.errorString());
      otaDone = 0;
    }
  }
}

void webServerInit() {
  server.on(
    "/update", HTTP_POST,
    []() {
      handleUpdateEnd();
    },
    []() {
      handleUpdate();
    });
  server.on("/favicon.ico", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
  });
  //  server.onNotFound([]() {
  //    server.send(200, "text/html", indexHtml);
  //  });

  server.on("/", handleRoot);
  server.on("/submitNumber", HTTP_GET, handleNumberSubmission);
  server.on("/submitSSID", HTTP_GET, handleSSIDSubmission);
  server.on("/toggle", handleToggle);

  server.begin();

  Serial.printf("Web Server ready at http://esp32.local or http://%s\n", WiFi.localIP().toString().c_str());

  tkSecond.attach(1, everySecond);
}

#endif
