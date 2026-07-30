#ifndef ConductimetroSTM32_h
#define ConductimetroSTM32_h

#include "Arduino.h"

class ConductimetroSTM32 {
  public:
    struct PlataformaConfig {
      float cellConstant;            // 1/cm (0.1, 1.0, 10.0)
      float tempAlpha;               // Coeficiente de compensacion termica
      float excitationFrequencyHz;   // Referencia para etapa de excitacion AC
      float excitationAmplitudeVpp;  // Referencia para amplitud de excitacion
    };

    struct LimitesConfig {
      float low_uScm;
      float high_uScm;
      float hysteresis_uScm;
      uint32_t alarmDelayMs;
    };

    struct DiagnosticoConfig {
      float minSupplyV;
      float maxSupplyV;
      float minSensorVrms;
      float maxSensorVrms;
      float minShuntVrms;
      float maxShuntVrms;
    };

    struct Muestra {
      uint32_t timestampMs;
      float sensorVrms;
      float shuntVrms;
      float shuntOhms;
      float temperaturaC;
      float supplyV;
    };

    struct Medicion {
      bool valid;
      float conductanceS;
      float conductivity_uScm;
      float conductivity25_uScm;
      float filtered_uScm;
    };

    struct PuntoCalibracion {
      float measured_uScm;
      float reference_uScm;
      bool enabled;
    };

    enum EventoTipo : uint8_t {
      EVENT_ALARM_HIGH_ON = 1,
      EVENT_ALARM_HIGH_OFF,
      EVENT_ALARM_LOW_ON,
      EVENT_ALARM_LOW_OFF,
      EVENT_SENSOR_OPEN,
      EVENT_SENSOR_SHORT,
      EVENT_SUPPLY_FAULT,
      EVENT_CALIBRATION_UPDATED,
      EVENT_CONFIG_UPDATED
    };

    struct Evento {
      EventoTipo tipo;
      uint32_t timestampMs;
      float value;
    };

    struct AlarmasEstado {
      bool high;
      bool low;
      bool sensorFaultOpen;
      bool sensorFaultShort;
      bool supplyFault;
    };

    typedef bool (*PersistWriteCallback)(const uint8_t* data, size_t size);
    typedef bool (*PersistReadCallback)(uint8_t* data, size_t size);

    ConductimetroSTM32();

    void begin(uint32_t nowMs = 0);

    void setPlataformaConfig(const PlataformaConfig& cfg);
    void setLimitesConfig(const LimitesConfig& cfg);
    void setDiagnosticoConfig(const DiagnosticoConfig& cfg);

    void setSecurityPin(uint32_t pin);
    bool updateCriticalConfig(uint32_t pin, const LimitesConfig& limites, const DiagnosticoConfig& diag, uint32_t timestampMs);

    void setPuntoCalibracion(uint8_t index, float measured_uScm, float reference_uScm, uint32_t timestampMs);
    uint8_t getCalibrationCount() const;

    Medicion processSample(const Muestra& muestra);
    const Medicion& getLastMeasurement() const;

    float applyTemperatureCompensation(float conductivity_uScm, float temperatureC) const;
    float applyCalibration(float conductivity25_uScm) const;

    AlarmasEstado getAlarmas() const;
    uint8_t getAlarmMask() const;

    bool saveConfiguration(PersistWriteCallback writer) const;
    bool loadConfiguration(PersistReadCallback reader);

    uint8_t copyEventos(Evento* out, uint8_t maxItems) const;

    int exportHoldingRegisters(uint16_t* regs, size_t maxRegs) const;

    static uint32_t computeCRC32(const uint8_t* data, size_t len);

  private:
    static const uint8_t kFilterWindow = 7;
    static const uint8_t kCalibracionPuntos = 3;
    static const uint8_t kEventosMax = 32;
    static const uint32_t kPersistMagic = 0x434F4E44UL; // "COND"
    static const uint16_t kPersistVersion = 1;

    struct PersistImage {
      uint32_t magic;
      uint16_t version;
      PlataformaConfig plataforma;
      LimitesConfig limites;
      DiagnosticoConfig diagnostico;
      PuntoCalibracion cal[kCalibracionPuntos];
      uint32_t securityPin;
      uint32_t crc32;
    };

    PlataformaConfig _plataforma;
    LimitesConfig _limites;
    DiagnosticoConfig _diagnostico;
    PuntoCalibracion _cal[kCalibracionPuntos];
    uint32_t _securityPin;

    Medicion _last;

    AlarmasEstado _alarmas;
    uint32_t _pendingHighSince;
    uint32_t _pendingLowSince;

    float _window[kFilterWindow];
    uint8_t _windowCount;
    uint8_t _windowIndex;

    Evento _eventos[kEventosMax];
    uint8_t _eventoHead;
    uint8_t _eventoCount;

    void pushEvento(EventoTipo tipo, uint32_t timestampMs, float value);
    float pushFilter(float value);
    float medianWindow() const;

    void updateDiagnostics(const Muestra& muestra);
    void updateAlarmas(uint32_t timestampMs, float conductivity_uScm);

    static float interpolate(float x0, float y0, float x1, float y1, float x);
};

#endif
