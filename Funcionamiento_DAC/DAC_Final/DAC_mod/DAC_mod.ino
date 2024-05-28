#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp32-hal-timer.h>

const char* ssid = "DroneTest";
const char* password = "12345678";
AsyncWebServer server(80);

const int dacPin = 25; // Pin where the LED is connected

// Variables to store the slider values
int slider1Value = 250;
int slider2Value = 100;

// Variables to store the frequencies
float frequency1 = 250.0;
float voltage2 = 1.0; // Default to 1.0V

// Timer variables
hw_timer_t *timer1 = NULL;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;

// Square wave state
bool squareWaveHigh = true;

const char* html = R"rawliteral(
<!DOCTYPE HTML>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Slider Control</title>
  <script>
    // Function to update frequency display in real-time
    function updateDisplay(channel, val) {
      document.getElementById("rate" + channel).textContent = val;
    }

    // Function to update slider value from text input
    function updateSlider(channel, val) {
      document.getElementById("slider" + channel).value = val;
    }

    // Function to send updated value to the server using POST
    function sendUpdate(channel) {
      const val = document.getElementById("slider" + channel).value;
      fetch("/slider" + channel, {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({ value: val })
      });
    }

    // Function to handle text input and update corresponding slider
    function handleInput(channel, event) {
      if (event.keyCode === 13) { // Enter key
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

    // Function to fetch and display current slider values
    async function fetchCurrentValues() {
      const response = await fetch('/currentValues');
      const data = await response.json();
      updateDisplay(1, data.slider1);
      updateSlider(1, data.slider1);
      updateDisplay(2, data.slider2);
      updateSlider(2, data.slider2);
    }

    document.addEventListener('DOMContentLoaded', fetchCurrentValues);
  </script>
</head>
<body>
  <h1>ESP32 ADC Sampling Rate</h1>

  <div>
    <h3>Frecuencia Canal 1: <span id="rate1">250</span> Hz</h3>
    <input type="range" id="slider1" min="0" max="500" value="250" 
           oninput="updateDisplay(1, this.value)" />
    <input type="text" onkeyup="handleInput(1, event)" value="250" />
    <button onclick="sendUpdate(1)">Actualizar</button>
  </div>
  <br>

  <div>
    <h3>Voltaje Canal 2: <span id="rate2">1.0</span> V</h3>
    <input type="range" id="slider2" min="0" max="3.3" step="0.1" value="1.0" 
           oninput="updateDisplay(2, this.value)" />
    <input type="text" onkeyup="handleInput(2, event)" value="1.0" />
    <button onclick="sendUpdate(2)">Actualizar</button>
  </div>

</body>
</html>
)rawliteral";

///////// funciones de timer /////////

// ISR for Timer1
void IRAM_ATTR onTimer1() {
  portENTER_CRITICAL_ISR(&timerMux1);
  if (squareWaveHigh) {
    dacWrite(dacPin, (int)(voltage2 / 3.3 * 255)); // Output specified voltage
  } else {
    dacWrite(dacPin, 0); // Output 0V
  }
  squareWaveHigh = !squareWaveHigh; // Toggle state
  portEXIT_CRITICAL_ISR(&timerMux1);
}

// Function to configure the timer based on the frequency
void configureTimer(hw_timer_t *&timer, int timerId, float frequency, void (*isr)(), portMUX_TYPE &mux) {
  if (timer == NULL) {
    timer = timerBegin(timerId, 80, true);
    timerAttachInterrupt(timer, isr, true);
  }
  int period = (int)(1000000 / (2 * frequency)); // Calculate half period for square wave
  timerAlarmWrite(timer, period, true);
  timerAlarmEnable(timer);
}

// Function to end the timer
void endTimer(hw_timer_t *&timer) {
  if (timer != NULL) {
    timerEnd(timer);
    timer = NULL;
  }
}

// Function to handle the body of POST requests
void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, int slider) {
  String body = "";
  for (size_t i = 0; i < len; i++) {
    body += (char) data[i];
  }
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, body);
  int value = doc["value"];
  if (slider == 1) {
    slider1Value = value;
    frequency1 = value;
    configureTimer(timer1, 0, frequency1, onTimer1, timerMux1); // Use timerId 0 for timer1
    Serial.println("Slider 1: " + String(slider1Value) + " Hz");
  } else if (slider == 2) {
    slider2Value = value;
    voltage2 = value;
    Serial.println("Slider 2: " + String(voltage2) + " V");
  }
  request->send(200, "text/plain", "OK");
}

void handleSlider1Post(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  handleBody(request, data, len, 1);
}

void handleSlider2Post(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  handleBody(request, data, len, 2);
}

void handleGetCurrentValues(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  doc["slider1"] = slider1Value;
  doc["slider2"] = voltage2;
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", html);
  });

  // Handle POST requests for slider1
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (request->url() == "/slider1" && request->method() == HTTP_POST) {
      handleSlider1Post(request, data, len, index, total);
    }
    if (request->url() == "/slider2" && request->method() == HTTP_POST) {
      handleSlider2Post(request, data, len, index, total);
    }
  });

  // Handle GET requests to retrieve current slider values
  server.on("/currentValues", HTTP_GET, handleGetCurrentValues);

  // Initial timer configuration
  configureTimer(timer1, 0, frequency1, onTimer1, timerMux1); // Use timerId 0 for timer1

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  // Handle other tasks here if needed
}
