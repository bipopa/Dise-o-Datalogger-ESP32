const int pinSalida = 26; // Pin donde conectaremos la señal de salida

void setup() {
  pinMode(pinSalida, OUTPUT);
}

void loop() {
  // Cambiar la frecuencia ajustando el valor de la variable 'periodo' (en microsegundos)
  int periodo = 1000000; // 1 segundo (1000000 microsegundos) de período
  digitalWrite(pinSalida, HIGH); // Establecer el pin de salida en alto
  delayMicroseconds(periodo / 2); // Esperar la mitad del período
  digitalWrite(pinSalida, LOW); // Establecer el pin de salida en bajo
  delayMicroseconds(periodo / 2); // Esperar la mitad del período
}
