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
    //variables para guardar datos
    var isRecording = false;
    var recordedData = [];
        function toggleRecording() {
            isRecording = !isRecording;
            if (isRecording) {
                recordedData = [];  // Resetear el array al comenzar la grabación
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
        };
///////////////// Logica actualización de graficas///////////
          function updateCharts() {
              fetch('/data').then(response => response.json()).then(data => {
                  const newTime = new Date().toLocaleTimeString(); // Obtiene la hora actual para usar en ambos gráficos.
                  
                  // Actualizar gráfico individual con sus respectivos datos
                  updateChart(chart1, newTime, data.adc1);
                  updateChart(chart2, newTime, data.adc2);
          
                  // Actualizar gráfico combinado
                  updateCombinedChart(chart3, newTime, data.adc1, data.adc2);
          
                  // Manejar la grabación de datos
                  if (isRecording) {
                      recordedData.push(`${newTime}, ${data.adc1}, ${data.adc2}\n`);
                  }
          
                  setTimeout(updateCharts, 1000); // Reinicia la función cada segundo
              }).catch(error => console.error('Error fetching data:', error));
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
///////////////Descargar Datos////////////////
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
      
//////////////////Funciones de mostrar datos///////////////
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
    <div id='chart1Container' class='hidden'><canvas id='adcChart1'></canvas></div>
    <div id='chart2Container' class='hidden'><canvas id='adcChart2'></canvas></div>
    <div id='chart3Container' class='hidden'><canvas id='adcChart3'></canvas></div>
</body>
</html>
    )rawliteral");
});

 server.on("/data", HTTP_GET, []() {
    int adcValue1 = adc1_get_raw(ADC1_CHANNEL_4);
    int adcValue2 = adc1_get_raw(ADC1_CHANNEL_5);
    
    // Convertir valores ADC a voltaje
    float voltage1 = (float)adcValue1 * 3.3 / 4095.0;
    float voltage2 = (float)adcValue2 * 3.3 / 4095.0;

    char json[128];  // Aumentar el tamaño para evitar cualquier desbordamiento
    snprintf(json, 128, "{\"adc1\": %.3f, \"adc2\": %.3f}", voltage1, voltage2);
    server.send(200, "application/json", json);
});

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
