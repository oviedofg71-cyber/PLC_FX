#ifndef PLC_FX_h
#define PLC_FX_h

#include "Arduino.h"
#include <HardwareSerial.h>

class PLC_FX {
  private:
    HardwareSerial* _serial;
    void enviarTrama(char cmd, String addr, String data, int lenBytes);
    String intToPLC(int valor);

  public:
    // Constructor: se le pasa el puerto serial (ej: Serial2)
    PLC_FX(HardwareSerial& port);
    
    // Inicializa la comunicación (opcional, para centralizar el begin)
    void begin(unsigned long baud = 19200);

    // Lectura de bits (X, Y, M, T)
    bool leerBit(char tipo, int numero);

    // Escritura de bits (Y, M)
    void escribirBit(char tipo, int numero, bool estado);

    // Escritura de registros D (16 bits)
    void escribirD(int numero, int valor);
};

#endif