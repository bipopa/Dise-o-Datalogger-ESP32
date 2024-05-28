#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <driver/adc.h>
#include <driver/timer.h>
#include <Preferences.h>  // Biblioteca para NVS

const char* ssid = "DroneTest";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

hw_timer_t *timer1 = NULL;
hw_timer_t *timer2 = NULL;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux2 = portMUX_INITIALIZER_UNLOCKED;

volatile float adc1_value = 0;
volatile float adc2_value = 0;
int slider1Value;
int slider2Value;
unsigned long lastSendTime = 0;

Preferences preferences;

void IRAM_ATTR onTimer1() {
  portENTER_CRITICAL_ISR(&timerMux1);
  int adcValue1 = adc1_get_raw(ADC1_CHANNEL_3);
  adc1_value = (float)adcValue1 * 3.3 / 4095.0;
  portEXIT_CRITICAL_ISR(&timerMux1);
}

void IRAM_ATTR onTimer2() {
  portENTER_CRITICAL_ISR(&timerMux2);
  int adcValue2 = adc1_get_raw(ADC1_CHANNEL_4);
  adc2_value = (float)adcValue2 * 3.3 / 4095.0;
  portEXIT_CRITICAL_ISR(&timerMux2);
}

void configureTimer(hw_timer_t *&timer, int timerId, float frequency, void (*isr)(), portMUX_TYPE &mux) {
  if (timer != NULL) {
    timerAlarmDisable(timer); // Disable the timer alarm
    timerDetachInterrupt(timer); // Detach the interrupt service routine
    timerEnd(timer); // End the timer
  }
  timer = timerBegin(timerId, 80, true);
  timerAttachInterrupt(timer, isr, true);
  int timerCount = round(1000000.0 / frequency); // Calculate and round the timer count
  timerAlarmWrite(timer, timerCount, true);
  timerAlarmEnable(timer);
}

void sendADCData() {
  DynamicJsonDocument doc(1024);
  portENTER_CRITICAL(&timerMux1);
  float adc1 = adc1_value;
  portEXIT_CRITICAL(&timerMux1);
  portENTER_CRITICAL(&timerMux2);
  float adc2 = adc2_value;
  portEXIT_CRITICAL(&timerMux2);

  doc["adc1"] = adc1;
  doc["adc2"] = adc2;
  String jsonString;
  serializeJson(doc, jsonString);
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, (char*)data);

    if (doc.containsKey("slider1")) {
      slider1Value = doc["slider1"];
      preferences.putInt("slider1Value", slider1Value); // Save slider1Value to NVS
      configureTimer(timer1, 0, slider1Value, onTimer1, timerMux1);
    }

    if (doc.containsKey("slider2")) {
      slider2Value = doc["slider2"];
      preferences.putInt("slider2Value", slider2Value); // Save slider2Value to NVS
      configureTimer(timer2, 1, slider2Value, onTimer2, timerMux2);
    }

    if (doc.containsKey("getCurrentValues")) {
      DynamicJsonDocument response(1024);
      response["slider1"] = slider1Value;
      response["slider2"] = slider2Value;
      String jsonString;
      serializeJson(response, jsonString);
      ws.textAll(jsonString);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client connected: %u\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client disconnected: %u\n", client->id());
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize NVS
  preferences.begin("settings", false);
  slider1Value = preferences.getInt("slider1Value", 250); // Load slider1Value from NVS, default 250
  slider2Value = preferences.getInt("slider2Value", 100); // Load slider2Value from NVS, default 100

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);  // GPIO32
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);  // GPIO33

  // Initialize timers with stored frequencies
  configureTimer(timer1, 0, slider1Value, onTimer1, timerMux1);
  configureTimer(timer2, 1, slider2Value, onTimer2, timerMux2);

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <style>
    .chart-container { width: 40vw; height: 60vh; display: inline-block; }
    .hidden { display: none; }
  </style>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <script>
    var isRecording = false;
    var recordedData = [];
    var ws;

    function toggleRecording() {
        isRecording = !isRecording;
        if (isRecording) {
            recordedData = [];
            document.getElementById('recordButton').innerText = 'Detener Grabación';
        } else {
            document.getElementById('recordButton').innerText = 'Iniciar Grabación';
            downloadData();
        }
    }

    var ctx1, chart1, ctx2, chart2, ctx3, chart3, viewMode1 = 1, viewMode2 = 1, viewMode3 = 1;
    window.onload = function() {
        wsConnect();
        ctx1 = document.getElementById('adcChart1').getContext('2d');
        chart1 = new Chart(ctx1, {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'Voltaje ADC1 (V)', data: [], borderColor: 'red' }] },
            options: { scales: { y: { beginAtZero: true, title: { display: true, text: 'Voltaje (V)' } } } }
        });
        ctx2 = document.getElementById('adcChart2').getContext('2d');
        chart2 = new Chart(ctx2, {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'Voltaje ADC2 (V)', data: [], borderColor: 'blue' }] },
            options: { scales: { y: { beginAtZero: true, title: { display: true, text: 'Voltaje (V)' } } } }
        });
        ctx3 = document.getElementById('adcChart3').getContext('2d');
        chart3 = new Chart(ctx3, {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'Voltaje ADC1 (V)', data: [], borderColor: 'red' }, { label: 'Voltaje ADC2 (V)', data: [], borderColor: 'blue' }] },
            options: { scales: { y: { beginAtZero: true, title: { display: true, text: 'Voltaje (V)' } } } }
        });
        requestCurrentValues();
    };

    function wsConnect() {
        ws = new WebSocket(`ws://${window.location.host}/ws`);
        ws.onopen = function() {
            console.log("WebSocket connected");
        };
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.slider1 !== undefined && data.slider2 !== undefined) {
                updateDisplay(1, data.slider1);
                updateSlider(1, data.slider1);
                updateDisplay(2, data.slider2);
                updateSlider(2, data.slider2);
            } else {
                const newTime = new Date().toLocaleTimeString();
                updateChart(chart1, newTime, data.adc1);
                updateChart(chart2, newTime, data.adc2);
                updateCombinedChart(chart3, newTime, data.adc1, data.adc2);
                if (isRecording) {
                    recordedData.push(`${newTime}, ${data.adc1}, ${data.adc2}\n`);
                }
            }
        };
        ws.onclose = function() {
            console.log("WebSocket disconnected, trying to reconnect");
            setTimeout(wsConnect, 1000);
        };
        ws.onerror = function(err) {
            console.error("WebSocket error", err);
            ws.close();
        };
    }

    function requestCurrentValues() {
        ws.send(JSON.stringify({ getCurrentValues: true }));
    }

    function updateChart(chart, newTime, newData) {
        chart.data.labels.push(newTime);
        chart.data.datasets[0].data.push(newData);
        if (chart.data.labels.length > 50) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }
        chart.update();
    }

    function updateCombinedChart(chart, newTime, newData1, newData2) {
        chart.data.labels.push(newTime);
        chart.data.datasets[0].data.push(newData1);
        chart.data.datasets[1].data.push(newData2);
        if (chart.data.labels.length > 50) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
            chart.data.datasets[1].data.shift();
        }
        chart.update();
    }

    function downloadData() {
        const blob = new Blob(recordedData, { type: 'text/plain' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.style.display = 'none';
        a.href = url;
        a.download = 'recorded_data.txt';
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        a.remove();
    }

    function toggleViewChart1() {
        viewMode1 = (viewMode1 + 1) % 2;
        document.getElementById('chart1Container').className = viewMode1 === 0 ? 'chart-container' : 'hidden';
    }

    function toggleViewChart2() {
        viewMode2 = (viewMode2 + 1) % 2;
        document.getElementById('chart2Container').className = viewMode2 === 0 ? 'chart-container' : 'hidden';
    }

    function toggleViewChart3() {
        viewMode3 = (viewMode3 + 1) % 2;
        document.getElementById('chart3Container').className = viewMode3 === 0 ? 'chart-container' : 'hidden';
    }

    function updateDisplay(channel, val) {
        document.getElementById("rate" + channel).textContent = val;
    }

    function updateSlider(channel, val) {
        document.getElementById("slider" + channel).value = val;
    }

    function sendUpdate(channel) {
        const val = document.getElementById("slider" + channel).value;
        ws.send(JSON.stringify({ [`slider${channel}`]: val }));
    }

    function handleInput(channel, event) {
        if (event.keyCode === 13) {
            const val = event.target.value;
            const slider = document.getElementById("slider" + channel);
            const min = parseInt(slider.min);
            const max = parseInt(slider.max);
            if (!isNaN(val) && val >= min && val <= max) {
                updateSlider(channel, val);
                updateDisplay(channel, val);
            } else {
                alert(`Please enter a valid number between ${min} and ${max}`);
            }
        }
    }
  </script>
</head>
<body>
  <h1>ESP32 ADC Readings</h1>
  <div>
    <button id="recordButton" onclick="toggleRecording()">Iniciar Grabación</button>
    <button onclick='toggleViewChart1()'>Mostrar Grafica 1</button>
    <button onclick='toggleViewChart2()'>Mostrar Grafica 2</button>
    <button onclick='toggleViewChart3()'>Mostrar Graficas Combinadas</button>
  </div>
  <div id='chart1Container' class='hidden'><canvas id='adcChart1'></canvas>
    <h3>Frecuencia Canal 1: <span id="rate1">250</span> Hz</h3>
    <input type="range" id="slider1" min="0" max="500" value="250" oninput="updateDisplay(1, this.value)" />
    <input type="text" onkeyup="handleInput(1, event)" value="250" />
    <button onclick="sendUpdate(1)">Actualizar</button>
  </div>
  <div id='chart2Container' class='hidden'><canvas id='adcChart2'></canvas>
    <h3>Frecuencia Canal 2: <span id="rate2">100</span> Hz</h3>
    <input type="range" id="slider2" min="0" max="500" value="100" oninput="updateDisplay(2, this.value)" />
    <input type="text" onkeyup="handleInput(2, event)" value="100" />
    <button onclick="sendUpdate(2)">Actualizar</button>
  </div>
  <div id='chart3Container' class='hidden'><canvas id='adcChart3'></canvas></div>
</body>
</html>
    )rawliteral");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  ws.cleanupClients();

  unsigned long currentTime = millis();
  if (currentTime - lastSendTime > 1000) { // Enviar datos cada segundo
    sendADCData();
    lastSendTime = currentTime;
  }
}
