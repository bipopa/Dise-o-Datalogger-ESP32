#include <esp32-hal-timer.h>

const int dacPin = 26; // Pin donde está conectado el LED
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
bool ledEncendido = false;
const uint32_t Ton = 1000000;  // 1 segundo
const uint32_t Toff = 200000;  // 0.5 segundo

void IRAM_ATTR controlarLED() {
  // Sección crítica para asegurar que el acceso al hardware esté protegido
  portENTER_CRITICAL_ISR(&timerMux);
  
  if (ledEncendido) {
    dacWrite(dacPin, 0);
    timerAlarmWrite(timer, Toff, true); // Cambiar el tiempo de apagado (0.5 segundos)
  } else {
    dacWrite(dacPin, 255);
    timerAlarmWrite(timer, Ton, true); // Cambiar el tiempo de encendido (1 segundo)
  }

  // Cambiar el estado del LED
  ledEncendido = !ledEncendido;

  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  //pinMode(pinLed, OUTPUT);

  // Inicializar el timer
  timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 tick = 1 microsegundo), cuenta ascendente
  timerAttachInterrupt(timer, &controlarLED, true); // Adjuntar la función de interrupción
  timerAlarmWrite(timer, Ton, true); // Inicialmente, configurar la alarma para 1 segundo
  timerAlarmEnable(timer); // Habilitar la alarma del timer
}

void loop() {
  // El loop puede contener otras operaciones, ya que el timer maneja el encendido y apagado del LED
}
