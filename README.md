# PLC_FX

Librería para comunicación con PLC Mitsubishi FX1N (lectura/escritura de X, Y, M, T, D) y módulo de arquitectura firmware para conductímetro profesional con STM32.

## Módulo PLC FX

Permite decodificar de forma simple operaciones de lectura y escritura sobre PLC FX:
- Lectura de bits: `leerBit(X|Y|M|T, numero)`
- Escritura de bits: `escribirBit(Y|M, numero, estado)`
- Lectura de registro: `leerD(numero)`
- Escritura de registro: `escribirD(numero, valor)`

Ejemplo: `/home/runner/work/PLC_FX/PLC_FX/examples/MonitorInteligente/MonitorInteligente.ino`

## Módulo Conductímetro STM32

Clase principal: `ConductimetroSTM32`

Archivo:
- `/home/runner/work/PLC_FX/PLC_FX/ConductimetroSTM32.h`
- `/home/runner/work/PLC_FX/PLC_FX/ConductimetroSTM32.cpp`

### Capacidades implementadas

1. **Plataforma base**
- Configuración de celda (K), compensación térmica, frecuencia y amplitud de excitación AC.

2. **Cadena de medición**
- Procesamiento de muestra RMS (`sensorVrms`, `shuntVrms`, `shuntOhms`) para calcular:
  - conductancia (S)
  - conductividad (uS/cm)
  - conductividad compensada a 25°C
- Filtrado digital combinado mediana + promedio móvil.

3. **Sensores auxiliares y diagnóstico**
- Monitoreo de tensión de alimentación.
- Detección de falla sensor abierto/corto por umbrales de señal.

4. **Firmware modular**
- Calibración multipunto (hasta 3 puntos) con interpolación lineal segmentada.
- Límites alto/bajo programables con histéresis y retardo anti-falsa alarma.
- Registro de eventos con buffer circular (alarmas, diagnóstico, calibración, cambios de config).
- Persistencia de parámetros con CRC32 (mediante callbacks de lectura/escritura para backend Flash/EEPROM).

5. **Interfaz y comunicación industrial**
- Exportación de 12 holding registers para integración Modbus RTU sobre RS-485 (`exportHoldingRegisters`).

6. **Seguridad y confiabilidad**
- Protección de cambios críticos mediante PIN (`updateCriticalConfig`).
- Verificación de integridad de configuración por `CRC32`.

### Ejemplo de uso

`/home/runner/work/PLC_FX/PLC_FX/examples/ConductimetroSTM32Demo/ConductimetroSTM32Demo.ino`

Demuestra:
- Configuración de plataforma/límites/diagnóstico.
- Carga de 3 puntos de calibración.
- Persistencia y restauración de configuración.
- Procesamiento periódico de muestra y lectura de estado de alarmas.


## Hardware del conductímetro (circuito electrónico)

Se añadió la propuesta de circuito electrónico completo (bloques, conexiones, componentes, valores iniciales, BOM y reglas PCB):

- `/home/runner/work/PLC_FX/PLC_FX/docs/ConductimetroSTM32_Circuito.md`
