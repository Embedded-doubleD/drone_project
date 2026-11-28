# Pruebas de robustez frente a fallos de sensores

Fecha de las pruebas: 27-nov-2026 · Rama: `feat/sensor-robustness` · Requisitos: GR3, SM5, NM5, SR5, AC5

## 1. Objetivo y metodo

Comprobar que el sistema sigue emitiendo tramas validas y reporta el fallo con los flags y el estado correctos cuando un sensor falla, sin bloquearse. Cada prueba se captura en modo SIMU (`capture_simu.py`) y se analiza con `flags_timeline.py`, `state_timeline.py` y `analyze_simu.py` (tiempos del reloj de la placa; resolucion de 100 ms, una trama). `state_timeline.py` descarta las tramas con checksum erroneo y cuenta cuantas descarta.

Estado base con todo conectado: `FLAGS = 0x00C0` (bits 0x40 y 0x80 que activa la IMU al calibrar). Sin fix GPS: `0x00C2`.

## 2. Matriz de pruebas

| # | Prueba | Esperado | Observado | Evidencia | Estado |
|---|---|---|---|---|---|
| 1 | Sin fix GPS, placa en reposo | `FLAGS=0x00C2`, estado IDLE, tramas continuas (NM5, SM5) | `FLAGS=0x00C2`, IDLE, tramas continuas | Capturas SIMU en `docs/evidence/` | Hecha |
| 2 | Desconexion y reconexion de la IMU | Bit `0x0001` activo mientras falla; PROBLEM al fallar; vuelta a IDLE tras recuperarse; tramas continuas | Bit `0x0001` de t=3,8 s a t=8,9 s; PROBLEM en la misma trama (+0,0 s); sale 3,0 s despues de borrarse el bit; 302/302 tramas, sin saltos | `imu_flags.txt` | Hecha |
| 3 | Corte de alimentacion del Si7021 | Bit `0x0008`; sin PROBLEM; tramas continuas; bit `0x0001` sin activarse | Bit `0x0008` de t=10,0 s a t=12,0 s (2,0 s); estado IDLE todo el tiempo; `0x0001` nunca activo; 304/304 tramas, mayor salto 100 ms | `si7021_flags.txt` | Hecha |
| 4 | Tapar y destapar la LDR | Luz baja al tapar y vuelve al destapar; sin bit `0x0004` | Tapada con la mano ~4,5 % (rango 4,2-4,8 %); destapada 47,5 %; bit `0x0004` nunca activo | `ldr_cover.txt` | Hecha (prueba funcional; extremos no medidos, ver seccion 6) |
| 5 | Desconectar la LDR | Bit `0x0004` si la lectura sale del rango calibrado; sin PROBLEM | Bit `0x0004` de t=83,0 s a t=89,0 s (6,0 s); estado IDLE | Primera captura de `ldr_disconnect.txt` | Hecha; repeticion limpia pendiente (ver 5.1) |

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

## 4. Prueba 3: corte de alimentacion del Si7021

`python tools/state_timeline.py si7021_flags.txt 0x0008`

```
t=    0.0 s  SEQ 00145  STATE IDLE      FLAGS 0x00C2 [GPS,SOLO_INERCIAL,YAW_NO_FIABLE]  (inicio)
t=   10.0 s  SEQ 00245  STATE IDLE      FLAGS 0x00CA  flags +[ENV] -[-]
t=   12.0 s  SEQ 00265  STATE IDLE      FLAGS 0x00C2  flags +[-] -[ENV]
304 tramas leidas (30.3 s)
```

`python tools/analyze_simu.py si7021_flags.txt` (extracto)

```
si7021_flags.txt: 304 tramas de ~304 esperadas por TMS; 0 saltos > 150 ms, el mayor de 100 ms
```

- El fallo del Si7021 se senala con el bit `0x0008` (ENV invalida) durante 2,0 s (20 tramas), sin cambiar el estado: PROBLEM queda reservado para IMU, impacto o rotacion critica, y la falta de un sensor ambiental no es una condicion de vuelo peligrosa.
- El bit de IMU (`0x0001`) no se activo: el modulo apagado no carga el bus I2C compartido.
- Las tramas siguieron saliendo con normalidad (SM5, GR3).
- Durante la prueba hubo movimiento de la placa (giroscopio maximo 10,3 deg/s), sin que apareciera FLYING.
- La duracion del bit depende de cuanto tiempo se mantuvo cortada la alimentacion y del periodo de lectura (2 s); no es una latencia del firmware.

## 5. Pruebas de la LDR

### 5.1 Prueba 5: desconexion de la LDR

`python tools/state_timeline.py ldr_disconnect.txt 0x0004` (primera captura)

```
t=   83.0 s  SEQ 02505  STATE IDLE      FLAGS 0x00C6  flags +[LDR] -[-]
t=   89.0 s  SEQ 02565  STATE IDLE      FLAGS 0x00C2  flags +[-] -[LDR]
```

- Al desconectar la LDR se activa el bit `0x0004` (LDR invalida) durante 6,0 s (60 tramas, de la 2505 a la 2565) y el estado sigue en IDLE. Al reconectar, el bit se borra. Se detecta y se senala la lectura fuera del rango calibrado (SR5, GR3).
- El evento cae en un tramo de tramas validas y consecutivas. La misma captura contiene mas atras un hueco de 76,3 s y una trama corrupta (dos tramas mezcladas en una linea), anteriores y ajenos al evento.

**Perdidas de tramas en la captura:** los huecos de las capturas de esta prueba son del PC, no de la placa. En la primera captura la secuencia avanza 763 tramas en 76,3 s; en la segunda (`ldr_disconnect.txt`, repeticion) salta de SEQ 1995 a 2166 (171 tramas) mientras el TMS salta de 211127 a 228227 ms (17,1 s). En ambos casos el salto de secuencia coincide con el del tiempo, es decir, la placa siguio emitiendo a 10 Hz sin pararse y fue la captura la que perdio tramas. En esa segunda captura no aparece ningun cambio de flags en las tramas validas, por lo que es inconcluyente y no se usa como evidencia. Recomendado para repetirla: comprobar con `lsof /dev/cu.usbmodem11303` que ningun otro proceso usa el puerto y lanzar `caffeinate -i python tools/capture_simu.py /dev/cu.usbmodem11303 30 ldr_disconnect.txt`.

### 5.2 Prueba 4: LDR tapada y destapada

Porcentaje de luz (valor del campo de luz de la trama SIMU) muestreado cada segundo; la LDR se lee cada 2 s, por eso los valores se repiten de dos en dos.

| Tramo | Luz mostrada |
|---|---|
| Antes de tapar (t = 22-25 s) | 44,6 % y 40,9 % |
| Tapada con la mano (t = 26-35 s) | 4,2-4,8 % (media ~4,5 %) |
| Destapada (t >= 36 s) | 47,5 % estable |

`python tools/state_timeline.py ldr_cover.txt 0x0004` no muestra ningun cambio de flags: el bit `0x0004` no se activa. La primera trama del fichero es de otra sesion (TMS 53037 frente a ~22000 en las siguientes), por lo que el script informa de una duracion negativa; no afecta a los porcentajes.

Interpretacion: la respuesta es clara y repetible (diez veces mayor destapada). Con la mano no se alcanza oscuridad total, asi que ~4,5 % no indica un error de calibracion.

## 6. Calibracion de la LDR y comprobacion de extremos

### 6.1 Por que hay que repetirla en cada entorno

`LDR_RAW_DARK` y `LDR_RAW_BRIGHT` (en `src/app/config.h`) fijan el 0 % y el 100 %. Dependen de la iluminacion del lugar, de la distancia de la fuente y del propio montaje del divisor, asi que los valores medidos en un entorno no sirven en otro. En este trabajo se calibraron para el entorno de las pruebas y se repetira la calibracion en clase.

Valores usados en estas pruebas: `LDR_RAW_DARK` = 700, `LDR_RAW_BRIGHT` = 3900, `LDR_CALIBRATED` = 1. Entorno y fecha de medida: Casa, 20261128(madrugada sin luz solar).

### 6.2 Procedimiento (a repetir en clase)

1. Anadir temporalmente, en `ldr_service_read()` justo despues de `telemetry_set_light(...)`, y solo en modo TEST (en SIMU no se puede imprimir texto, GR5 y SM6):
   ```c
   /* TEMPORAL: solo para calibrar la LDR; quitar antes de commitear */
   if (fsm_get_mode() == FSM_MODE_TEST) {
       printk("LDR raw=%u\n", (unsigned)event.adc_raw);
   }
   ```
2. Medir unos 60 s por condicion (unas 30 lecturas, una cada 2 s): oscuridad (vaso opaco en una habitacion oscura), luz ambiente y luz maxima (linterna fija a unos 5 cm). Repetir cada condicion al menos dos veces.
3. Obtener estadisticos y propuesta de constantes con `tools/ldr_stats.py`:
   ```bash
   python tools/ldr_stats.py --port /dev/cu.usbmodem11303 --seconds 60 --save oscuridad_1.txt
   python tools/ldr_stats.py --dark oscuridad_*.txt --bright luz_*.txt --ambient ambiente_1.txt
   ```
   Propone `DARK = mediana de oscuridad - 3 desviaciones` y `BRIGHT = mediana de luz + 3 desviaciones` (maximo 4095), para que el ruido no marque la LDR como invalida. Avisa si la luz maxima satura el ADC o si el divisor esta invertido (la formula supone que el valor sube con la luz).
4. Poner los valores en `config.h` con `LDR_CALIBRATED` = 1 y quitar el `printk`.
5. Comprobar los extremos con capturas SIMU de 20 s (`ldr_dark.txt`, `ldr_bright.txt`):
   ```bash
   awk -F, '/^\$SIMU1/{s+=$24;n++; if(min==""||$24<min)min=$24; if($24>max)max=$24} END{print "media",s/n,"min",min,"max",max}' ldr_dark.txt
   python tools/state_timeline.py ldr_dark.txt 0x0004
   ```
   Criterio de aceptacion propuesto por el equipo (no viene del enunciado): oscuridad <= 3 %, luz maxima >= 97 % y sin bit `0x0004`.

### 6.3 Estado en esta entrega

La comprobacion de extremos (6.2, paso 5) **no se hizo** en este entorno, porque los valores se volveran a calibrar en clase. Si en la sesion final ya existen los valores definitivos, se ejecuta ese procedimiento.

## 7. Limitaciones

- Resolucion temporal de 100 ms (una trama).
- Una sola placa; los cortes se hicieron a mano, sin repetibilidad.
- Con el GPS sin fix no se midio el comportamiento con fix.
- Durante el fallo, los valores del sensor afectado en la trama quedan congelados en el ultimo valido y solo el flag indica el fallo (pendiente de comprobar en una captura).
- Los estadisticos de `analyze_simu.py` no descartan tramas con checksum erroneo; con capturas corruptas pueden salir picos espurios.
- La calibracion de la LDR no se ha verificado en los extremos y es especifica del entorno.
