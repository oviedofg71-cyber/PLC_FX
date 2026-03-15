#include <PLC_FX.h>

// Instancia de la libreria usando el puerto Serial2
PLC_FX plc(Serial2);

bool ultimoX0 = false;
bool ultimoY5 = false;
bool primeraVez = true;

void setup() {
  Serial.begin(115200);
  // Configuracion tipica para PLC FX1N: 19200, 7E1
  Serial2.begin(19200, SERIAL_7E1, 16, 17);
  Serial.println(">>> PLC_FX: Ejemplo de Monitor iniciado <<<");
}

void loop() {
  // Lectura de pines
  bool estadoX0 = plc.leerBit('X', 0);
  bool estadoY5 = plc.leerBit('Y', 5);

  // Deteccion de cambios de estado
  if (primeraVez || (estadoX0 != ultimoX0) || (estadoY5 != ultimoY5)) {
    Serial.print(primeraVez ? "[ESTADO INICIAL] " : "[CAMBIO DETECTADO] ");
    Serial.print("X0: "); Serial.print(estadoX0 ? "ON " : "OFF");
    Serial.print(" | Y5: "); Serial.println(estadoY5 ? "ON " : "OFF");

    ultimoX0 = estadoX0;
    ultimoY5 = estadoY5;
    primeraVez = false;
  }
  delay(100);
}