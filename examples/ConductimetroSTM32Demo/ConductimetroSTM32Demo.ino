#include <ConductimetroSTM32.h>
#include <string.h>

ConductimetroSTM32 conductimetro;

static uint8_t flashImage[256];

bool flashWrite(const uint8_t* data, size_t size) {
  if (size > sizeof(flashImage)) return false;
  memcpy(flashImage, data, size);
  return true;
}

bool flashRead(uint8_t* data, size_t size) {
  if (size > sizeof(flashImage)) return false;
  memcpy(data, flashImage, size);
  return true;
}

void setup() {
  Serial.begin(115200);

  ConductimetroSTM32::PlataformaConfig plataforma = {1.0f, 0.019f, 1000.0f, 1.2f};
  ConductimetroSTM32::LimitesConfig limites = {10.0f, 1800.0f, 25.0f, 1500};
  ConductimetroSTM32::DiagnosticoConfig diag = {4.7f, 5.3f, 0.001f, 2.8f, 0.0005f, 2.0f};

  conductimetro.setPlataformaConfig(plataforma);
  conductimetro.setLimitesConfig(limites);
  conductimetro.setDiagnosticoConfig(diag);
  conductimetro.setSecurityPin(4321);

  conductimetro.setPuntoCalibracion(0, 84.0f, 84.0f, millis());
  conductimetro.setPuntoCalibracion(1, 1413.0f, 1413.0f, millis());
  conductimetro.setPuntoCalibracion(2, 12880.0f, 12880.0f, millis());

  conductimetro.saveConfiguration(flashWrite);
  conductimetro.loadConfiguration(flashRead);

  Serial.println("Conductimetro STM32 demo iniciado");
}

void loop() {
  const uint32_t now = millis();

  ConductimetroSTM32::Muestra muestra;
  muestra.timestampMs = now;
  muestra.sensorVrms = 0.52f;
  muestra.shuntVrms = 0.018f;
  muestra.shuntOhms = 100.0f;
  muestra.temperaturaC = 24.8f;
  muestra.supplyV = 5.02f;

  ConductimetroSTM32::Medicion m = conductimetro.processSample(muestra);
  ConductimetroSTM32::AlarmasEstado alarmas = conductimetro.getAlarmas();

  if (m.valid) {
    Serial.print("Cond25(uS/cm): ");
    Serial.print(m.conductivity25_uScm, 2);
    Serial.print(" | Filtrada: ");
    Serial.print(m.filtered_uScm, 2);
    Serial.print(" | AlarmMask: ");
    Serial.println(conductimetro.getAlarmMask());
  }

  if (alarmas.high || alarmas.low || alarmas.sensorFaultOpen || alarmas.sensorFaultShort || alarmas.supplyFault) {
    Serial.println("Alarma/diagnostico activo");
  }

  delay(1000);
}
