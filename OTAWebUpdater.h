#ifndef OTAWEBUPDATER_H_
#define OTAWEBUPDATER_H_

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Ticker.h>

#include "html.h"

WebServer server(80);
Ticker tkSecond;
uint8_t otaDone = 0;

int switch1;
int relay_off_hour = 19;
int relay_on_hour = 8;
int relay_off_min = 0;
int relay_on_min = 0;

String ssid = "openwrt-2g";
String password = "1qaz2wsx";
int save_nvs = 0;

void onSwitch1Change(int on);
void wifiLed_toggle(void);

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
  html += "relay off hour(0~23): <input type=\"number\" name=\"offHour\" min=\"0\" max=\"23\" value=\"" + String(relay_off_hour) + "\">";
  html += " minute(0~59): <input type=\"number\" name=\"offMin\" min=\"0\" max=\"59\" value=\"" + String(relay_off_min) + "\">";
  html += "<br>";
  html += "relay on hour(0~23): <input type=\"number\" name=\"onHour\" min=\"0\" max=\"23\" value=\"" + String(relay_on_hour) + "\">";
  html += " minute(0~59): <input type=\"number\" name=\"onMin\" min=\"0\" max=\"59\" value=\"" + String(relay_on_min) + "\">";
  html += "<br>";
  html += "<input type=\"submit\" value=\"Submit\">";
  html += "<br>";
  html += "=====================================";
  html += "</form>";

  // ssid and password
  html += "IP is" + WiFi.localIP().toString();
  html += "<br>";
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
  int tmp1, tmp2, tmp3, tmp4;

  if ((server.hasArg("offHour")) && (server.hasArg("onHour")) && (server.hasArg("offMin")) && (server.hasArg("onMin"))) {
    tmp1 = server.arg("offHour").toInt();
    tmp2 = server.arg("onHour").toInt();
    tmp3 = server.arg("offMin").toInt();
    tmp4 = server.arg("onMin").toInt();

    server.sendHeader("Refresh", "10");
    server.sendHeader("Location", "/");
    server.send(307);

    // check value are valid ?
    if (((tmp1 >= 0) && (tmp1 <= 23)) && ((tmp2 >= 0) && (tmp2 <= 23)) && ((tmp3 >= 0) && (tmp3 <= 59)) && ((tmp4 >= 0) && (tmp4 <= 59))) {
      relay_off_hour = tmp1;
      relay_on_hour = tmp2;
      relay_off_min = tmp3;
      relay_on_min = tmp4;
      save_nvs = 1;
    }

    Serial.printf("received time hour for relay off: %d:%d, relay on: %d:%d \n", relay_off_hour, relay_off_min, relay_on_hour, relay_on_min);
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
      Serial.printf("received ssid: %s, passwd: %s \n", ssid, password);
      save_nvs = 2;
    } else {
      Serial.printf("received invalid ssid: %s, passwd: %s \n", ssid, password);
    }

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
    wifiLed_toggle();
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
