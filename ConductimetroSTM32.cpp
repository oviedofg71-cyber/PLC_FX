#include "ConductimetroSTM32.h"

#include <math.h>
#include <string.h>

ConductimetroSTM32::ConductimetroSTM32() {
  _plataforma = {1.0f, 0.019f, 1000.0f, 1.0f};
  _limites = {500.0f, 1500.0f, 20.0f, 2000};
  _diagnostico = {4.75f, 5.25f, 0.001f, 3.0f, 0.0005f, 2.5f};

  for (uint8_t i = 0; i < kCalibracionPuntos; i++) {
    _cal[i] = {0.0f, 0.0f, false};
  }

  _securityPin = 1234;
  _last = {false, 0.0f, 0.0f, 0.0f, 0.0f};
  _alarmas = {false, false, false, false, false};

  _pendingHighSince = 0;
  _pendingLowSince = 0;

  _windowCount = 0;
  _windowIndex = 0;
  for (uint8_t i = 0; i < kFilterWindow; i++) _window[i] = 0.0f;

  _eventoHead = 0;
  _eventoCount = 0;
}

void ConductimetroSTM32::begin(uint32_t nowMs) {
  (void)nowMs;
  _pendingHighSince = 0;
  _pendingLowSince = 0;
}

void ConductimetroSTM32::setPlataformaConfig(const PlataformaConfig& cfg) {
  _plataforma = cfg;
}

void ConductimetroSTM32::setLimitesConfig(const LimitesConfig& cfg) {
  _limites = cfg;
}

void ConductimetroSTM32::setDiagnosticoConfig(const DiagnosticoConfig& cfg) {
  _diagnostico = cfg;
}

void ConductimetroSTM32::setSecurityPin(uint32_t pin) {
  _securityPin = pin;
}

bool ConductimetroSTM32::updateCriticalConfig(uint32_t pin, const LimitesConfig& limites, const DiagnosticoConfig& diag, uint32_t timestampMs) {
  if (pin != _securityPin) return false;
  _limites = limites;
  _diagnostico = diag;
  pushEvento(EVENT_CONFIG_UPDATED, timestampMs, 0.0f);
  return true;
}

void ConductimetroSTM32::setPuntoCalibracion(uint8_t index, float measured_uScm, float reference_uScm, uint32_t timestampMs) {
  if (index >= kCalibracionPuntos) return;
  _cal[index].measured_uScm = measured_uScm;
  _cal[index].reference_uScm = reference_uScm;
  _cal[index].enabled = (measured_uScm > 0.0f && reference_uScm > 0.0f);
  pushEvento(EVENT_CALIBRATION_UPDATED, timestampMs, reference_uScm);
}

uint8_t ConductimetroSTM32::getCalibrationCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kCalibracionPuntos; i++) {
    if (_cal[i].enabled) count++;
  }
  return count;
}

ConductimetroSTM32::Medicion ConductimetroSTM32::processSample(const Muestra& muestra) {
  _last.valid = false;

  if (muestra.sensorVrms <= 0.0f || muestra.shuntVrms < 0.0f || muestra.shuntOhms <= 0.0f) {
    updateDiagnostics(muestra);
    return _last;
  }

  const float currentA = muestra.shuntVrms / muestra.shuntOhms;
  const float conductanceS = (muestra.sensorVrms > 0.0f) ? (currentA / muestra.sensorVrms) : 0.0f;
  const float conductivity_uScm = conductanceS * _plataforma.cellConstant * 1000000.0f;
  const float conductivity25 = applyTemperatureCompensation(conductivity_uScm, muestra.temperaturaC);
  const float calibrated25 = applyCalibration(conductivity25);
  const float filtered = pushFilter(calibrated25);

  _last.valid = true;
  _last.conductanceS = conductanceS;
  _last.conductivity_uScm = conductivity_uScm;
  _last.conductivity25_uScm = calibrated25;
  _last.filtered_uScm = filtered;

  updateDiagnostics(muestra);
  updateAlarmas(muestra.timestampMs, filtered);

  return _last;
}

const ConductimetroSTM32::Medicion& ConductimetroSTM32::getLastMeasurement() const {
  return _last;
}

float ConductimetroSTM32::applyTemperatureCompensation(float conductivity_uScm, float temperatureC) const {
  const float factor = 1.0f + (_plataforma.tempAlpha * (temperatureC - 25.0f));
  if (factor <= 0.0f) return conductivity_uScm;
  return conductivity_uScm / factor;
}

float ConductimetroSTM32::applyCalibration(float conductivity25_uScm) const {
  PuntoCalibracion points[kCalibracionPuntos];
  uint8_t n = 0;

  for (uint8_t i = 0; i < kCalibracionPuntos; i++) {
    if (_cal[i].enabled) points[n++] = _cal[i];
  }

  if (n == 0) return conductivity25_uScm;

  for (uint8_t i = 0; i < n; i++) {
    for (uint8_t j = i + 1; j < n; j++) {
      if (points[j].measured_uScm < points[i].measured_uScm) {
        PuntoCalibracion tmp = points[i];
        points[i] = points[j];
        points[j] = tmp;
      }
    }
  }

  if (n == 1) {
    if (points[0].measured_uScm <= 0.0f) return conductivity25_uScm;
    return conductivity25_uScm * (points[0].reference_uScm / points[0].measured_uScm);
  }

  if (conductivity25_uScm <= points[0].measured_uScm) {
    return interpolate(points[0].measured_uScm, points[0].reference_uScm,
                       points[1].measured_uScm, points[1].reference_uScm,
                       conductivity25_uScm);
  }

  if (conductivity25_uScm >= points[n - 1].measured_uScm) {
    return interpolate(points[n - 2].measured_uScm, points[n - 2].reference_uScm,
                       points[n - 1].measured_uScm, points[n - 1].reference_uScm,
                       conductivity25_uScm);
  }

  for (uint8_t i = 0; i < n - 1; i++) {
    if (conductivity25_uScm >= points[i].measured_uScm && conductivity25_uScm <= points[i + 1].measured_uScm) {
      return interpolate(points[i].measured_uScm, points[i].reference_uScm,
                         points[i + 1].measured_uScm, points[i + 1].reference_uScm,
                         conductivity25_uScm);
    }
  }

  return conductivity25_uScm;
}

ConductimetroSTM32::AlarmasEstado ConductimetroSTM32::getAlarmas() const {
  return _alarmas;
}

uint8_t ConductimetroSTM32::getAlarmMask() const {
  uint8_t mask = 0;
  if (_alarmas.high) mask |= 1 << 0;
  if (_alarmas.low) mask |= 1 << 1;
  if (_alarmas.sensorFaultOpen) mask |= 1 << 2;
  if (_alarmas.sensorFaultShort) mask |= 1 << 3;
  if (_alarmas.supplyFault) mask |= 1 << 4;
  return mask;
}

bool ConductimetroSTM32::saveConfiguration(PersistWriteCallback writer) const {
  if (!writer) return false;

  PersistImage image;
  image.magic = kPersistMagic;
  image.version = kPersistVersion;
  image.plataforma = _plataforma;
  image.limites = _limites;
  image.diagnostico = _diagnostico;
  image.securityPin = _securityPin;
  for (uint8_t i = 0; i < kCalibracionPuntos; i++) image.cal[i] = _cal[i];

  const size_t crcBytes = sizeof(PersistImage) - sizeof(uint32_t);
  image.crc32 = computeCRC32((const uint8_t*)&image, crcBytes);

  return writer((const uint8_t*)&image, sizeof(PersistImage));
}

bool ConductimetroSTM32::loadConfiguration(PersistReadCallback reader) {
  if (!reader) return false;

  PersistImage image;
  if (!reader((uint8_t*)&image, sizeof(PersistImage))) return false;

  if (image.magic != kPersistMagic || image.version != kPersistVersion) return false;

  const size_t crcBytes = sizeof(PersistImage) - sizeof(uint32_t);
  const uint32_t calcCrc = computeCRC32((const uint8_t*)&image, crcBytes);
  if (calcCrc != image.crc32) return false;

  _plataforma = image.plataforma;
  _limites = image.limites;
  _diagnostico = image.diagnostico;
  _securityPin = image.securityPin;
  for (uint8_t i = 0; i < kCalibracionPuntos; i++) _cal[i] = image.cal[i];

  return true;
}

uint8_t ConductimetroSTM32::copyEventos(Evento* out, uint8_t maxItems) const {
  if (!out || maxItems == 0) return 0;

  uint8_t copied = 0;
  const uint8_t count = (_eventoCount < maxItems) ? _eventoCount : maxItems;

  for (uint8_t i = 0; i < count; i++) {
    uint8_t index = (_eventoHead + kEventosMax - count + i) % kEventosMax;
    out[copied++] = _eventos[index];
  }

  return copied;
}

int ConductimetroSTM32::exportHoldingRegisters(uint16_t* regs, size_t maxRegs) const {
  if (!regs || maxRegs < 12) return 0;

  regs[0] = (uint16_t)constrain((int)roundf(_last.filtered_uScm * 10.0f), 0, 65535);      // uS/cm x10
  regs[1] = (uint16_t)constrain((int)roundf(_last.conductivity25_uScm * 10.0f), 0, 65535); // uS/cm x10
  regs[2] = (uint16_t)constrain((int)roundf(_limites.low_uScm * 10.0f), 0, 65535);
  regs[3] = (uint16_t)constrain((int)roundf(_limites.high_uScm * 10.0f), 0, 65535);
  regs[4] = (uint16_t)constrain((int)roundf(_plataforma.cellConstant * 1000.0f), 0, 65535); // K x1000
  regs[5] = (uint16_t)constrain((int)roundf(_plataforma.tempAlpha * 10000.0f), 0, 65535);   // alpha x10000
  regs[6] = (uint16_t)constrain((int)roundf(_plataforma.excitationFrequencyHz), 0, 65535);
  regs[7] = (uint16_t)constrain((int)roundf(_plataforma.excitationAmplitudeVpp * 1000.0f), 0, 65535);
  regs[8] = _limites.alarmDelayMs > 65535 ? 65535 : (uint16_t)_limites.alarmDelayMs;
  regs[9] = getAlarmMask();
  regs[10] = _last.valid ? 1 : 0;
  regs[11] = getCalibrationCount();

  return 12;
}

uint32_t ConductimetroSTM32::computeCRC32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return ~crc;
}

void ConductimetroSTM32::pushEvento(EventoTipo tipo, uint32_t timestampMs, float value) {
  _eventos[_eventoHead] = {tipo, timestampMs, value};
  _eventoHead = (_eventoHead + 1) % kEventosMax;
  if (_eventoCount < kEventosMax) _eventoCount++;
}

float ConductimetroSTM32::pushFilter(float value) {
  _window[_windowIndex] = value;
  _windowIndex = (_windowIndex + 1) % kFilterWindow;
  if (_windowCount < kFilterWindow) _windowCount++;

  const float med = medianWindow();

  float sum = 0.0f;
  for (uint8_t i = 0; i < _windowCount; i++) sum += _window[i];
  const float avg = (_windowCount > 0) ? (sum / (float)_windowCount) : med;

  return (med + avg) * 0.5f;
}

float ConductimetroSTM32::medianWindow() const {
  if (_windowCount == 0) return 0.0f;

  float tmp[kFilterWindow];
  for (uint8_t i = 0; i < _windowCount; i++) tmp[i] = _window[i];

  for (uint8_t i = 0; i < _windowCount; i++) {
    for (uint8_t j = i + 1; j < _windowCount; j++) {
      if (tmp[j] < tmp[i]) {
        float t = tmp[i];
        tmp[i] = tmp[j];
        tmp[j] = t;
      }
    }
  }

  if ((_windowCount & 1) == 1) return tmp[_windowCount / 2];
  return (tmp[(_windowCount / 2) - 1] + tmp[_windowCount / 2]) * 0.5f;
}

void ConductimetroSTM32::updateDiagnostics(const Muestra& muestra) {
  const bool wasOpen = _alarmas.sensorFaultOpen;
  const bool wasShort = _alarmas.sensorFaultShort;
  const bool wasSupply = _alarmas.supplyFault;

  _alarmas.sensorFaultOpen = (muestra.sensorVrms < _diagnostico.minSensorVrms) ||
                             (muestra.shuntVrms < _diagnostico.minShuntVrms);

  _alarmas.sensorFaultShort = (muestra.sensorVrms > _diagnostico.maxSensorVrms) ||
                              (muestra.shuntVrms > _diagnostico.maxShuntVrms);

  _alarmas.supplyFault = (muestra.supplyV < _diagnostico.minSupplyV) ||
                         (muestra.supplyV > _diagnostico.maxSupplyV);

  if (!wasOpen && _alarmas.sensorFaultOpen) pushEvento(EVENT_SENSOR_OPEN, muestra.timestampMs, muestra.sensorVrms);
  if (!wasShort && _alarmas.sensorFaultShort) pushEvento(EVENT_SENSOR_SHORT, muestra.timestampMs, muestra.sensorVrms);
  if (!wasSupply && _alarmas.supplyFault) pushEvento(EVENT_SUPPLY_FAULT, muestra.timestampMs, muestra.supplyV);
}

void ConductimetroSTM32::updateAlarmas(uint32_t timestampMs, float conductivity_uScm) {
  if (!_alarmas.high) {
    if (conductivity_uScm >= _limites.high_uScm) {
      if (_pendingHighSince == 0) _pendingHighSince = timestampMs;
      if ((timestampMs - _pendingHighSince) >= _limites.alarmDelayMs) {
        _alarmas.high = true;
        pushEvento(EVENT_ALARM_HIGH_ON, timestampMs, conductivity_uScm);
      }
    } else {
      _pendingHighSince = 0;
    }
  } else if (conductivity_uScm <= (_limites.high_uScm - _limites.hysteresis_uScm)) {
    _alarmas.high = false;
    _pendingHighSince = 0;
    pushEvento(EVENT_ALARM_HIGH_OFF, timestampMs, conductivity_uScm);
  }

  if (!_alarmas.low) {
    if (conductivity_uScm <= _limites.low_uScm) {
      if (_pendingLowSince == 0) _pendingLowSince = timestampMs;
      if ((timestampMs - _pendingLowSince) >= _limites.alarmDelayMs) {
        _alarmas.low = true;
        pushEvento(EVENT_ALARM_LOW_ON, timestampMs, conductivity_uScm);
      }
    } else {
      _pendingLowSince = 0;
    }
  } else if (conductivity_uScm >= (_limites.low_uScm + _limites.hysteresis_uScm)) {
    _alarmas.low = false;
    _pendingLowSince = 0;
    pushEvento(EVENT_ALARM_LOW_OFF, timestampMs, conductivity_uScm);
  }
}

float ConductimetroSTM32::interpolate(float x0, float y0, float x1, float y1, float x) {
  if (fabsf(x1 - x0) < 0.000001f) return y0;
  return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}
