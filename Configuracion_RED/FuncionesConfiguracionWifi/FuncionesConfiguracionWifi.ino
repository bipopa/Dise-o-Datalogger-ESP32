#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Crear una instancia del servidor web en el puerto 80
WebServer server(80);

// Crear una instancia de Preferences para almacenar la configuración de Wi-Fi
Preferences preferences;


void handleRoot() {
// Función para manejar la página raíz
 String html = "<html><body><h1>Pagina de Inicio</h1>"
                "<button onclick=\"location.href='https//google.com'\">Modo Datalogger</button><br>"
                "<button onclick=\"location.href='/config'\">Configuracion de Red</button>"
                "</body></html>";
  server.send(200, "text/html", html);
}

// Función para manejar la página raíz
void handleConfig() {
  String html = "<html><body><h1>Configuracion de WiFi</h1>"
                "<form action=\"/configStatus\" method=\"POST\">"
                "SSID: <input type=\"text\" name=\"ssid\"><br>"
                "Password: <input type=\"text\" name=\"password\"><br>"
                "<input type=\"submit\" value=\"Guardar\"><br>"
                "</form>""<button onclick=\"location.href='/status'\">Estado de  Red</button><br>"
                "<button onclick=\"location.href='/'\">inicio</button>"
                "</body></html>";
                
  server.send(200, "text/html", html);
}

// Función para manejar la página de estado de conexion
void handleStatus() {
  String html = "<html><body><h1>Estado de la conexion WiFi</h1>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<p>Nombre de la red (SSID): " + WiFi.SSID() + "</p>";
    html += "<p>Direccion IP: " + WiFi.localIP().toString() + "</p>"
    "<button onclick=\"location.href='/'\">inicio</button>";
    
  } else {
    html += "<p>No esta conectado a ninguna red WiFi.</p>"
    "<button onclick=\"location.href='/'\">inicio</button>";
    
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Función para manejar la configuración de WiFi
void handleConfigStatus() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // Guardar SSID y password en la memoria persistente
  preferences.begin("wifi-config", false); // Nombre del espacio de almacenamiento y modo de escritura
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  // Intentar conectar a la red WiFi
  WiFi.begin(ssid.c_str(), password.c_str());

  // Esperar hasta que se conecte o falle
  int timeout = 20; // Tiempo de espera máximo de 20 segundos
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    timeout--;
  }

  String html;
  if (WiFi.status() == WL_CONNECTED) {
    html = "<html><body><h1>Configuracion guardada</h1>"
           "<p>Conectado a la red WiFi.</p>"
           "<p>IP Address: " + WiFi.localIP().toString() + "</p>"
           "<button onclick=\"location.href='/'\">inicio</button>";
           "</body></html>";
    Serial.println("");
    Serial.println("Conectado a la red WiFi.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    html = "<html><body><h1>Error de conexion</h1>"
           "<p>No se pudo conectar a la red WiFi. Por favor, intente nuevamente.</p>"
           "<button onclick=\"location.href='/'\">inicio</button>";
           "</body></html>";
    Serial.println("");
    Serial.println("No se pudo conectar a la red WiFi.");
  }

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Configurar el punto de acceso
  const char* ap_ssid = "ESP32_Config";
  const char* ap_password = "password";
  
  WiFi.softAP(ap_ssid, ap_password);

  // Imprimir la dirección IP del punto de acceso
  Serial.println("Punto de acceso iniciado.");
  Serial.print("Nombre de la red (SSID): ");
  Serial.println(ap_ssid);
  Serial.print("Dirección IP del punto de acceso: ");
  Serial.println(WiFi.softAPIP());

  // Iniciar la librería Preferences
  preferences.begin("wifi-config", true); // Nombre del espacio de almacenamiento y modo de lectura

  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/configStatus", HTTP_POST, handleConfigStatus);
  server.on("/status", handleStatus); // Nueva ruta para el estado de conexión

  // Iniciar el servidor
  server.begin();
  Serial.println("Servidor iniciado.");

  // Leer y conectar a la red Wi-Fi guardada, si está disponible
  String saved_ssid = preferences.getString("ssid", "");
  String saved_password = preferences.getString("password", "");

  if (saved_ssid != "" && saved_password != "") {
    Serial.println("Intentando conectar a la red WiFi guardada...");
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    Serial.println("Conectado a la red WiFi.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  // Manejar las peticiones del servidor web
  server.handleClient();
}
