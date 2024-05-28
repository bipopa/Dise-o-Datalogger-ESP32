#include <WiFi.h>
#include <WebServer.h>
#include <driver/adc.h>

const char* ssid = "DroneTest";
const char* password = "12345678";

WebServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);  // GPIO32
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);  // GPIO33

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<script>"
      "setInterval(function() {"
      "  fetch('/data').then(response => response.json()).then(data => {"
      "    document.getElementById('adc1').innerText = data.adc1;"
      "    document.getElementById('adc2').innerText = data.adc2;"
      "  });"
      "}, 1000);"
      "</script>"
      "</head>"
      "<body>"
      "<h1>ESP32 ADC Reading</h1>"
      "<p>ADC Channel 1 (GPIO 32): <span id='adc1'>--</span></p>"
      "<p>ADC Channel 2 (GPIO 33): <span id='adc2'>--</span></p>"
      "</body>"
      "</html>");
  });

  server.on("/data", HTTP_GET, []() {
    int adcValue1 = adc1_get_raw(ADC1_CHANNEL_4);
    int adcValue2 = adc1_get_raw(ADC1_CHANNEL_5);
    char json[75];
    snprintf(json, 75, "{\"adc1\": %d, \"adc2\": %d}", adcValue1, adcValue2);
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
