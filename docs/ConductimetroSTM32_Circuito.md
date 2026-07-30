# Circuito electrónico propuesto: Conductímetro STM32 (diálisis / ósmosis)

Este documento completa la parte de hardware para el módulo `ConductimetroSTM32`.

## 1) Arquitectura por bloques

1. **Alimentación aislada**
   - Entrada: 12–24 VDC.
   - Conversión primaria: 5 V para potencia digital.
   - Conversión aislada para medición analógica: 5V_ISO.
   - LDOs de bajo ruido:
     - 3V3_DIG (STM32 + lógica)
     - 3V3_ANA (ADC/AFE)

2. **Excitación AC de celda**
   - Salida DAC/PWM del STM32 -> filtro pasabajos activo.
   - Señal senoidal 1 kHz inicial (ajustable 500 Hz–5 kHz).
   - Driver de baja distorsión para inyectar señal a la celda.

3. **Cadena analógica de medición (AFE)**
   - Celda de conductividad 2 hilos (K seleccionable 0.1 / 1.0 / 10.0).
   - Resistencia shunt de precisión en serie para medir corriente de celda.
   - Canal V_cell: medición diferencial de tensión en celda.
   - Canal V_shunt: medición diferencial en shunt.
   - Filtro anti-alias antes del ADC.

4. **Conversión ADC de alta resolución**
   - ADC delta-sigma 24 bits (SPI) para V_cell, V_shunt y temperatura.
   - Referencia de voltaje de precisión dedicada.

5. **Sensado de temperatura**
   - RTD PT1000 (recomendado) en puente/corriente constante de precisión.
   - Alternativa: NTC 10k con red de linealización.

6. **Comunicaciones y alarmas**
   - RS-485 aislado (Modbus RTU).
   - Salida alarma por relé/SSR aislado.
   - Buzzer + LED de estado.

7. **Seguridad y robustez**
   - Watchdog + BOR del STM32.
   - TVS en entrada de alimentación y línea RS-485.
   - Separación de masas: DGND, AGND, ISO_GND con unión controlada.

---

## 2) Selección de componentes recomendados

- **MCU**: STM32G474RE (alternativa: STM32F303RE).
- **ADC**: ADS124S08 (24-bit, multicanal, PGA, RTD-friendly) o ADS1220 (2 canales).
- **Op-amp AFE**: OPA2188 / OPA2192 (bajo offset, baja deriva).
- **Referencia ADC**: REF5025 (2.5 V, baja deriva).
- **Shunt de corriente**: 100 ohm, 0.1%, 15 ppm/°C (valor inicial).
- **RS-485 aislado**: ADM2587E (aislado integrado) o MAX14840 + aislador digital.
- **DC/DC aislado**: 5V->5V aislado 1 W (p.ej. Murata/NME0505SC equivalente).
- **LDO bajo ruido**: TPS7A20 / LP5907 para 3V3_ANA.
- **Protecciones**:
  - TVS SMBJ para entrada de alimentación.
  - TVS bidireccional para A/B de RS-485.

---

## 3) Esquema funcional (conexiones clave)

## 3.1 Excitación AC

- `STM32_DAC1_OUT1` -> `R=1k` -> filtro activo 2º orden (fc ~2.5 kHz) -> buffer op-amp -> `EXC+`.
- `EXC-` referenciado a `ISO_GND` mediante red simétrica para minimizar componente DC.

## 3.2 Celda + shunt

- `EXC+` -> `CELDA+`.
- `CELDA-` -> `R_SHUNT` -> `ISO_GND`.
- Medición diferencial:
  - `AIN_VCELL_P` en `CELDA+`
  - `AIN_VCELL_N` en `CELDA-`
  - `AIN_SHUNT_P` arriba de `R_SHUNT`
  - `AIN_SHUNT_N` abajo de `R_SHUNT`

## 3.3 Filtrado de entrada ADC

- En cada canal diferencial: `Rserie=100 ohm` por rama + `Cdiff=10 nF` entre entradas.
- Filtro RC común-mode: `Ccm=1 nF` desde cada rama a `AGND`.

## 3.4 Temperatura PT1000

- Fuente de corriente de excitación RTD: 500 uA (precisión 0.1%).
- PT1000 a entrada diferencial dedicada del ADC.
- Resistencia de referencia de precisión para ratiométrico.

## 3.5 RS-485 y alarmas

- `USARTx_TX/RX` STM32 -> transceiver RS-485 aislado.
- Terminación: `120 ohm` seleccionable por jumper.
- Bias fail-safe: red 680 ohm/680 ohm (si transceiver no lo integra).
- GPIO STM32 -> opto/driver -> relé SSR de alarma.

---

## 4) Valores iniciales de diseño (arranque de prototipo)

- Frecuencia de excitación: **1 kHz**.
- Amplitud inicial en celda: **~1.0 Vpp**.
- Rango célula K=1.0: **50 uS/cm a 20 mS/cm** (ajustable por firmware y ganancia).
- Ganancia front-end: iniciar en **1x** y escalar por software/ADC PGA.
- Muestreo: 20–50 SPS para estabilidad metrológica.

---

## 5) BOM inicial (mínima para prototipo)

1. STM32G474RE (x1)
2. ADS124S08 (x1)
3. OPA2188 (x2)
4. REF5025 (x1)
5. DC/DC aislado 5V->5V 1W (x1)
6. LDO 3V3_DIG (x1), LDO 3V3_ANA (x1)
7. Transceiver RS-485 aislado (x1)
8. R shunt 100 ohm 0.1% (x1)
9. Resistencias 0.1% varias (filtros/puentes)
10. C0G/X7R para filtros y desacoplo
11. TVS alimentación + TVS RS-485
12. Conector celda, conector PT1000, borneras alimentación/RS-485

---

## 6) Reglas PCB (críticas)

- Plano analógico separado de digital; unión en punto estrella.
- Mantener lazo de medición de celda/shunt corto y simétrico.
- Alejar RS-485 y relé de entradas analógicas.
- Colocar referencia ADC y filtro cerca del ADC.
- Guard ring en nodos de alta impedancia si aplica.
- Desacoplos de 100 nF por pin de alimentación + bulk local.

---

## 7) Mapeo con firmware ya implementado

- `Muestra.sensorVrms` <- canal RMS de `V_cell`.
- `Muestra.shuntVrms` <- canal RMS de `V_shunt`.
- `Muestra.shuntOhms` <- valor real medido/calibrado de shunt.
- `Muestra.temperaturaC` <- RTD/NTC compensada.
- `Muestra.supplyV` <- monitor de línea 5V/3V3.

La lógica de calibración, compensación, alarmas, diagnóstico, eventos y CRC ya está implementada en:
- `/home/runner/work/PLC_FX/PLC_FX/ConductimetroSTM32.h`
- `/home/runner/work/PLC_FX/PLC_FX/ConductimetroSTM32.cpp`
