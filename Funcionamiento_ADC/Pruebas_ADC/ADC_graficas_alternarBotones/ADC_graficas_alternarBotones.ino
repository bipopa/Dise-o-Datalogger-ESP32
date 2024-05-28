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
      "<style>"
      ".chart-container { width: 40vw; height: 60vh; display: inline-block; }"
      ".hidden { display: none; }"
      "</style>"
      "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"
      "<script>"
      "var ctx1, chart1, ctx2, chart2, ctx3, chart3, viewMode1 = 1;viewMode2 = 1;viewMode3 = 1;"
      "window.onload = function() {"
      "  ctx1 = document.getElementById('adcChart1').getContext('2d');"
      "  chart1 = new Chart(ctx1, {"
      "    type: 'line',"
      "    data: { labels: [], datasets: [{ label: 'ADC1', data: [], borderColor: 'red' }] },"
      "    options: { scales: { y: { beginAtZero: true } } }"
      "  });"
      "  ctx2 = document.getElementById('adcChart2').getContext('2d');"
      "  chart2 = new Chart(ctx2, {"
      "    type: 'line',"
      "    data: { labels: [], datasets: [{ label: 'ADC2', data: [], borderColor: 'blue' }] },"
      "    options: { scales: { y: { beginAtZero: true } } }"
      "  });"
      "  ctx3 = document.getElementById('adcChart3').getContext('2d');"
      "  chart3 = new Chart(ctx3, {"
      "    type: 'line',"
      "    data: { labels: [], datasets: [{ label: 'ADC1', data: [], borderColor: 'red' }, { label: 'ADC2', data: [], borderColor: 'blue' }] },"
      "    options: { scales: { y: { beginAtZero: true } } }"
      "  });"
      "  updateCharts();"
      "};"
      "function updateCharts() {"
      "  fetch('/data').then(response => response.json()).then(data => {"
      "    updateChart(chart1, data.adc1);"
      "    updateChart(chart2, data.adc2);"
      "    updateChart(chart3, data.adc1, data.adc2);"
      "    setTimeout(updateCharts, 1000);"
      "  });"
      "}"
      "function updateChart(chart, newData1, newData2 = null) {"
      "  if (chart.data.labels.length > 20) {"
      "    chart.data.labels.shift();"
      "    chart.data.datasets[0].data.shift();"
      "    if (newData2 !== null) chart.data.datasets[1].data.shift();"
      "  }"
      "  chart.data.labels.push('');"
      "  chart.data.datasets[0].data.push(newData1);"
      "  if (newData2 !== null) chart.data.datasets[1].data.push(newData2);"
      "  chart.update();"
      "}"
      //Botones mostrar graficas
      "function toggleViewChart1() {"
      "  viewMode1 = (viewMode1 + 1) % 2;"
      "  document.getElementById('chart1Container').className = viewMode1 === 0 ? 'chart-container' : 'hidden';"
      "}"
      
      "function toggleViewChart2() {"
      "  viewMode2 = (viewMode2 + 1) % 2;"
      "  document.getElementById('chart2Container').className = viewMode2 === 0 ? 'chart-container' : 'hidden';"
      "}"
      
      "function toggleViewChart3() {"
      "  viewMode3 = (viewMode3 + 1) % 2;"
      "  document.getElementById('chart3Container').className = viewMode3 === 0 ? 'chart-container' : 'hidden';"
      "}"
      "</script>"
      "</head>"
      "<body>"
      "<h1>ESP32 ADC Readings</h1>"
      "<div>"
      "<button onclick='toggleViewChart1()'>Mostrar Grafica 1</button>"
      "<button onclick='toggleViewChart2()'>Mostrar Grafica 2</button>"
      "<button onclick='toggleViewChart3()'>Mostrar Graficas Combinadas</button>""</div>"
      "<div id='chart1Container' class='hidden'><canvas id='adcChart1'></canvas></div>"
      "<div id='chart2Container' class='hidden'><canvas id='adcChart2'></canvas></div>"
      "<div id='chart3Container' class='hidden'><canvas id='adcChart3'></canvas></div>"
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
