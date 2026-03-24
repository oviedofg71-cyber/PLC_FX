#include "PLC_FX.h"

PLC_FX::PLC_FX(HardwareSerial& port) {
    _serial = &port;
}

void PLC_FX::begin(unsigned long baud) {
    _serial->begin(baud, SERIAL_7E1);
}

void PLC_FX::enviarTrama(char cmd, String addr, String data, int lenBytes) {
    String lenStr = (lenBytes < 10) ? "0" + String(lenBytes) : String(lenBytes);
    String payload = String(cmd) + addr + lenStr + data; 
    
    int suma = 0;
    for (int i = 0; i < payload.length(); i++) suma += payload[i];
    suma += 0x03; // ETX
    
    String cksum = String(suma & 0xFF, HEX);
    cksum.toUpperCase();
    if (cksum.length() < 2) cksum = "0" + cksum;

    _serial->write(0x02); // STX
    _serial->print(payload);
    _serial->write(0x03); // ETX
    _serial->print(cksum);
}

String PLC_FX::intToPLC(int valor) {
    char buf[5];
    sprintf(buf, "%04X", valor);
    String s = String(buf);
    return s.substring(2,4) + s.substring(0,2); // Little Endian
}

bool PLC_FX::leerBit(char tipo, int numero) {
    String baseAddr;
    tipo = toupper(tipo);
    if (tipo == 'X') baseAddr = "0080";
    else if (tipo == 'Y') baseAddr = "00A0";
    else if (tipo == 'M') baseAddr = "0100";
    else if (tipo == 'T') baseAddr = "00C0";
    else return false;

    int direccionInt = strtol(baseAddr.c_str(), NULL, 16) + (numero / 8);
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();

    while(_serial->available()) _serial->read();
    enviarTrama('0', addr, "", 1);
    
    delay(50);
    String res = "";
    while(_serial->available()) res += (char)_serial->read();

    if (res.length() >= 3) {
        int valorDec = strtol(res.substring(1, 3).c_str(), NULL, 16);
        return (valorDec & (1 << (numero % 8)));
    }
    return false;
}

void PLC_FX::escribirBit(char tipo, int numero, bool estado) {
    String baseAddr = (toupper(tipo) == 'M') ? "0100" : "00A0";
    int direccionInt = strtol(baseAddr.c_str(), NULL, 16) + (numero / 8);
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();

    int actual = 0;
    enviarTrama('0', addr, "", 1);
    delay(45);
    if(_serial->available()){
        String res = "";
        while(_serial->available()) res += (char)_serial->read();
        if(res.length() >= 3) actual = strtol(res.substring(1, 3).c_str(), NULL, 16);
    }

    if (estado) actual |= (1 << (numero % 8));
    else actual &= ~(1 << (numero % 8));

    String dataHex = String(actual, HEX);
    dataHex.toUpperCase();
    if(dataHex.length() < 2) dataHex = "0" + dataHex;
    enviarTrama('1', addr, dataHex, 1);
}

void PLC_FX::escribirD(int numero, int valor) {
    int direccionInt = 0x1000 + (numero * 2);
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();
    enviarTrama('1', addr, intToPLC(valor), 2);
}

int PLC_FX::leerD(int numero) {
    // Cada D ocupa 2 bytes, la dirección base es 1000h
    int direccionInt = 0x1000 + (numero * 2); 
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();

    while(_serial->available()) _serial->read();
    
    // Pedimos 2 bytes (16 bits)
    enviarTrama('0', addr, "", 2); 
    
    delay(50);
    String res = "";
    while(_serial->available()) res += (char)_serial->read();

    if (res.length() >= 5) {
        // El PLC responde en formato Little Endian (Byte bajo, Byte alto)
        // Ejemplo para valor 1 (0001): Responde "0100"
        String hexVal = res.substring(1, 5);
        String byteBajo = hexVal.substring(0, 2);
        String byteAlto = hexVal.substring(2, 4);
        
        // Reorganizamos y convertimos a entero
        return (int)strtol((byteAlto + byteBajo).c_str(), NULL, 16);
    }
    return -1; // Error de lectura
}

int PLC_FX::leerValorT(int numero) {
    // En FX2N, el valor acumulado de los T empieza en 0x0800
    int direccionInt = 0x0800 + (numero * 2); 
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();

    while(_serial->available()) _serial->read();
    enviarTrama('0', addr, "", 2); 
    
    delay(60);
    String res = "";
    while(_serial->available()) res += (char)_serial->read();

    if (res.length() >= 5) {
        String hexVal = res.substring(1, 5);
        String byteBajo = hexVal.substring(0, 2);
        String byteAlto = hexVal.substring(2, 4);
        return (int)strtol((byteAlto + byteBajo).c_str(), NULL, 16);
    }
    return -1;
}
