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
    else if (tipo == 'M') baseAddr = "0800";
    else if (tipo == 'T') baseAddr = "00C0";
    else return false;

    int direccionInt = strtol(baseAddr.c_str(), NULL, 16) + (numero / 8);
    String addr = String(direccionInt, HEX);
    while(addr.length() < 4) addr = "0" + addr;
    addr.toUpperCase();

    while(_serial->available()) _serial->read();
    enviarTrama('0', addr, "", 1);
    
    delay(45);
    String res = "";
    while(_serial->available()) res += (char)_serial->read();

    if (res.length() >= 3) {
        int valorDec = strtol(res.substring(1, 3).c_str(), NULL, 16);
        return (valorDec & (1 << (numero % 8)));
    }
    return false;
}

void PLC_FX::escribirBit(char tipo, int numero, bool estado) {
    String baseAddr = (toupper(tipo) == 'M') ? "0800" : "00A0";
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