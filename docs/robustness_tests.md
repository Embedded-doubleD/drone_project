# Pruebas de robustez frente a fallos de sensores

Fecha: 27-nov-2026 · Rama: `feat/sensor-robustness` · Requisitos: GR3, SM5, NM5, AC5

## 1. Objetivo y metodo

Comprobar que el sistema sigue emitiendo tramas validas y reporta el fallo con los flags y el estado correctos cuando un sensor falla, sin bloquearse. Cada prueba se captura en modo SIMU (`capture_simu.py`) y se analiza con `flags_timeline.py`, `state_timeline.py` y `analyze_simu.py` (tiempos del reloj de la placa; resolucion de 100 ms, una trama).

Estado base con todo conectado: `FLAGS = 0x00C0` (bits 0x40 y 0x80 que activa la IMU al calibrar). Sin fix GPS: `0x00C2`.

## 2. Matriz de pruebas

| # | Prueba | Esperado | Observado | Evidencia | Estado |
|---|---|---|---|---|---|
| 1 | Sin fix GPS, placa en reposo | `FLAGS=0x00C2`, estado IDLE, tramas continuas (NM5, SM5) | `FLAGS=0x00C2`, IDLE, tramas continuas | Capturas SIMU en `docs/evidence/` | Hecha |
| 2 | Desconexion y reconexion de la IMU | Bit `0x0001` activo mientras falla; PROBLEM al fallar; vuelta a IDLE tras recuperarse; tramas continuas | Bit `0x0001` de t=3,8 s a t=8,9 s; PROBLEM en la misma trama (+0,0 s); sale de PROBLEM 3,0 s despues de borrarse el bit; 302/302 tramas, sin saltos | `imu_flags.txt` y salidas del apartado 3 | Hecha |
| 3 | Corte de alimentacion del Si7021 | Bit `0x0008`; sin PROBLEM; tramas continuas; bit `0x0001` sin activarse | - | - | Pendiente |
| 4 | Tapar y destapar la LDR | ~0 % y ~100 % sin bit `0x0004` | - | - | Pendiente |
| 5 | Desconectar la LDR | Bit `0x0004` si la lectura sale del rango calibrado; sin PROBLEM | - | - | Pendiente |

## 3. Prueba 2: desconexion y reconexion de la IMU

Captura de 30 s en SIMU con la IMU desconectada y reconectada a mano.

`python tools/flags_timeline.py imu_flags.txt`

```
t=    0.0 s  SEQ 01642  FLAGS 0x00C2  [GPS,SOLO_INERCIAL,YAW_NO_FIABLE]  (inicio)
t=    3.8 s  SEQ 01680  FLAGS 0x00C3  activa: IMU  borra: -
t=    8.9 s  SEQ 01731  FLAGS 0x00C2  activa: -  borra: IMU
302 tramas leidas
```

`python tools/state_timeline.py imu_flags.txt`

```
t=    0.0 s  SEQ 01642  STATE IDLE      FLAGS 0x00C2 [GPS,SOLO_INERCIAL,YAW_NO_FIABLE]  (inicio)
t=    3.8 s  SEQ 01680  STATE PROBLEM   FLAGS 0x00C3  estado IDLE -> PROBLEM; flags +[IMU] -[-]
t=    8.9 s  SEQ 01731  STATE PROBLEM   FLAGS 0x00C2  flags +[-] -[IMU]
t=   11.9 s  SEQ 01761  STATE IDLE      FLAGS 0x00C2  estado PROBLEM -> IDLE
302 tramas leidas (30.1 s)
Bit activado -> PROBLEM: +0.0 s
Bit borrado -> sale de PROBLEM: +3.0 s
```

`python tools/analyze_simu.py imu_flags.txt` (extracto)

```
imu_flags.txt: 302 tramas de ~302 esperadas por TMS; 0 saltos > 150 ms, el mayor de 100 ms
  Z de +0.00 a +0.00 m; VZ de -0.01 a +0.00 m/s
```

### 3.1 Resultados

| Medida | Valor |
|---|---|
| IMU invalida (bit `0x0001` activo) | 5,1 s (51 tramas, de la 1680 a la 1731); incluye el tiempo que tarda la persona en desconectar y reconectar, no es latencia del firmware |
| Bit activado a entrada en PROBLEM | +0,0 s (misma trama; resolucion 100 ms) |
| Bit borrado a salida de PROBLEM | +3,0 s (30 tramas) |
| Continuidad de tramas (SM5) | 302 de ~302, 0 saltos mayores de 150 ms, maximo 100 ms |
| Posicion Z durante el fallo | Sin deriva (0,00 m) |
| Estados recorridos | IDLE, PROBLEM, IDLE; no aparece FLYING pese al movimiento al manipular los cables |

### 3.2 Interpretacion (diseno justificado)

- **Entrada inmediata en PROBLEM:** con la IMU caida no hay datos fiables de movimiento; mantener FLYING con datos viejos seria inseguro. Se trata como fallo significativo (NM5), no como persistente.
- **Salida lenta (3 s de datos sanos, constante del modulo de estado del tracker):** evita parpadeos si el contacto es intermitente (histeresis).
- **Tramas continuas:** el sistema sigue emitiendo tramas validas con un sensor caido y lo senala con flags (SM5, GR3).
- **Sin FLYING falso:** el tracker exige movimiento sostenido durante 1 s antes de pasar a FLYING; los movimientos breves al manipular los cables no lo activan.
- **Alcance de la prueba:** confirma que la implementacion se comporta como se diseno; no demuestra que 3 s sea el valor optimo, que es una decision de diseno.

## 4. Pruebas pendientes (protocolo)

### 4.1 Si7021 (corte de alimentacion)

1. `python tools/capture_simu.py /dev/cu.usbmodem11303 30 si7021_flags.txt`
2. A los ~5 s cortar la alimentacion del modulo (no SDA, que se comparte con la IMU) y reconectarla hacia los ~15 s.
3. `python tools/state_timeline.py si7021_flags.txt 0x0008` y `python tools/analyze_simu.py si7021_flags.txt`.
4. Esperado: bit `0x0008` en pocos segundos (periodo de lectura 2 s), estado IDLE, tramas continuas. Si el bit `0x0001` tambien se activa, el modulo apagado carga el bus I2C compartido: documentarlo como hallazgo.

### 4.2 LDR

1. Capturar `ldr_cover.txt` (30 s): tapar unos 10 s y destapar.
2. Ver el porcentaje: `awk -F, '/^\$SIMU1/{print $3, $24}' ldr_cover.txt | sed -n '1~10p'`.
3. Esperado: ~0 % tapada y ~100 % con luz, sin bit `0x0004`.
4. Repetir desconectando el cable de senal y anotar si aparece el bit `0x0004`; si no aparece, documentarlo como limitacion.

## 5. Limitaciones

- Resolucion temporal de 100 ms (una trama).
- Una sola placa; los cortes se hicieron a mano, sin repetibilidad.
- Con el GPS sin fix no se midio el comportamiento con fix.
- Durante el fallo, los valores de la IMU en la trama quedan congelados en el ultimo valido y solo el flag indica el fallo.
