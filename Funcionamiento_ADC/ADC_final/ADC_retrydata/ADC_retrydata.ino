#include <WiFi.h>
#include <WebServer.h>
#include <driver/adc.h>
#include <driver/timer.h>
#include <ArduinoJson.h>

const char* ssid = "DroneTest";
const char* password = "12345678";

WebServer server(80);

hw_timer_t *timer1 = NULL;
hw_timer_t *timer2 = NULL;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux2 = portMUX_INITIALIZER_UNLOCKED;

volatile float adc1_value = 0;
volatile float adc2_value = 0;
int slider1Value = 250;
int slider2Value = 100;

volatile float adc1_current_value=0;
volatile float adc2_current_value=0;

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
    timerEnd(timer);
  }
  timer = timerBegin(timerId, 80, true);
  timerAttachInterrupt(timer, isr, true);
  timerAlarmWrite(timer, 1000000 / frequency, true);
  timerAlarmEnable(timer);
}

void handleSlider1Post() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, body);
    int value = doc["value"];
    slider1Value = value;
    configureTimer(timer1, 0, slider1Value, onTimer1, timerMux1);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSlider2Post() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, body);
    int value = doc["value"];
    slider2Value = value;
    configureTimer(timer2, 1, slider2Value, onTimer2, timerMux2);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleGetCurrentValues() {
  String json;
  StaticJsonDocument<200> doc;
  doc["slider1"] = slider1Value;
  doc["slider2"] = slider2Value;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);  // GPIO32
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);  // GPIO33

  // Initialize timers with default frequencies
  configureTimer(timer1, 0, slider1Value, onTimer1, timerMux1);
  configureTimer(timer2, 1, slider2Value, onTimer2, timerMux2);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", R"rawliteral(
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
        updateCharts();
        fetchCurrentValues();
    };

    async function updateCharts() {
        try {
            const data = await fetchDataWithRetry('/data', 5, 1000);
            const newTime = new Date().toLocaleTimeString();
            updateChart(chart1, newTime, data.adc1);
            updateChart(chart2, newTime, data.adc2);
            updateCombinedChart(chart3, newTime, data.adc1, data.adc2);
            if (isRecording) {
                recordedData.push(`${newTime}, ${data.adc1}, ${data.adc2}\n`);
            }
            setTimeout(updateCharts, 1000);
        } catch (error) {
            console.error('Error fetching data:', error);
            // Opcional: mostrar un mensaje de error en la página
        }
    }

    async function fetchDataWithRetry(url, retries, delay) {
        for (let i = 0; i < retries; i++) {
            try {
                const response = await fetch(url);
                if (!response.ok) throw new Error('Network response was not ok');
                const data = await response.json();
                return data;
            } catch (error) {
                console.warn(`Attempt ${i + 1} failed: ${error.message}`);
                if (i < retries - 1) await new Promise(resolve => setTimeout(resolve, delay));
            }
        }
        throw new Error('Failed to fetch data after multiple attempts');
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
        postDataWithRetry(`/slider${channel}`, { value: val }, 5, 1000);
    }

    async function postDataWithRetry(url, data, retries, delay) {
        for (let i = 0; i < retries; i++) {
            try {
                const response = await fetch(url, {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify(data)
                });
                if (!response.ok) throw new Error('Network response was not ok');
                return await response.text();
            } catch (error) {
                console.warn(`Attempt ${i + 1} failed: ${error.message}`);
                if (i < retries - 1) await new Promise(resolve => setTimeout(resolve, delay));
            }
        }
        throw new Error('Failed to post data after multiple attempts');
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

    async function fetchCurrentValues() {
        const response = await fetch('/currentValues');
        const data = await response.json();
        updateDisplay(1, data.slider1);
        updateSlider(1, data.slider1);
        updateDisplay(2, data.slider2);
        updateSlider(2, data.slider2);
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

  server.on("/data", HTTP_GET, []() {
    char json[128];
    portENTER_CRITICAL(&timerMux1);
    float adc1 = adc1_value;
    portEXIT_CRITICAL(&timerMux1);
    portENTER_CRITICAL(&timerMux2);
    float adc2 = adc2_value;
    portEXIT_CRITICAL(&timerMux2);
    snprintf(json, 128, "{\"adc1\": %.3f, \"adc2\": %.3f}", adc1, adc2);
    server.send(200, "application/json", json);
  });

  server.on("/slider1", HTTP_POST, handleSlider1Post);
  server.on("/slider2", HTTP_POST, handleSlider2Post);
  server.on("/currentValues", HTTP_GET, handleGetCurrentValues);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
