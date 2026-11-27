# Fase 1: validacion de la telemetria SIMU y de los umbrales de reposo

Rama: `feat/sensor-robustness` · Fecha de las pruebas: 27-nov-2026 · Estado: cerrada (valores sin cambio, justificados con medidas)

## 1. Objetivo y criterio de exito

Comprobar con medidas reales que (a) la trama `$SIMU1` llega completa y a 10 Hz y (b) los umbrales con los que el firmware decide que la placa esta quieta (aceleracion lineal 0,5 m/s2, giroscopio 3 deg/s, 25 muestras = 0,5 s a 50 Hz) estan justificados por datos y no son valores a ojo.

Criterio: sin errores ni saltos de secuencia, periodo de 100 +-5 ms segun el reloj de la placa, y umbrales con margen sobre el peor reposo medido.

## 2. Herramientas (carpeta `tools/`)

| Script | Funcion |
|---|---|
| `check_simu.py` | Valida tramas (checksum, secuencia, periodo segun TMS). |
| `capture_simu.py` | Guarda las lineas `$SIMU1` del puerto serie en un fichero. |
| `analyze_simu.py` | Estadisticas de reposo y movimiento; opciones `--gref`, `--seg`, `--per-second`, `--tol`, `--gyro`. Informa de huecos, orientacion mediana y rachas por debajo de umbral (total e interior). |
| `flags_timeline.py` | Muestra cuando se activa o borra cada bit de `FLAGS`. |

Limitacion conocida: el giroscopio se publica con una decimal y truncado, asi que valores por debajo de 0,1 deg/s aparecen como 0,0. Las estadisticas de giroscopio de esta fase son las de la trama, no las internas del firmware.

## 3. Prueba 1: integridad de la trama

| Medida | Resultado |
|---|---|
| `capture_simu.py`, 20 s | 202 tramas guardadas |
| `check_simu.py`, 20 s | 186 validas, 0 con errores, 0 saltos de secuencia (SEQ 1437 a 1622) |
| Periodo segun TMS | min 100 ms, max 100 ms, media 100,0 ms; 0 fuera de 100 +-5 ms |
| Periodo medio en el PC | 98,6 ms (depende de USB y PC, no es un dato de la placa) |

Conclusion: la trama cumple. `check_simu.py` cuenta menos tramas porque empieza despues de abrir el puerto.

## 4. Prueba 2: umbrales de reposo

### 4.1 Metodo

Todas las capturas en la misma sesion, sin reiniciar la placa entre ellas (la gravedad calibrada al arrancar, 8,974 m/s2 = mediana de |a| en plano, se mantiene). Los reposos inclinados se apoyaron en un soporte, sin sujetarlos con la mano. Todos los analisis usan `--gref 8.974`, que es lo que ve el firmware.

Una primera tanda de reposos inclinados se descarto: el giroscopio tenia mediana de 0,5 y 0,73 deg/s (placa en la mano) y uno de los ficheros tenia unos 20 s sin tramas. Se repitieron con soporte.

### 4.2 Reposo

| Fichero | Orientacion mediana | Tramas | Lin p99 / max (m/s2) | Gyro max (deg/s) | Z (m) |
|---|---|---|---|---|---|
| `reposo_plano.txt` | plano | 603 en 60,2 s | 0,063 / 0,082 | < 0,1 | 0,00 constante |
| `reposo_pitch.txt` | pitch +18,3 deg | 305/305 | 0,208 / 0,229 | 0,224 | -0,69 a -0,64 |
| `reposo_roll30.txt` | roll +26,4 deg | 305/305 | 0,088 / 0,091 | 0,224 | -1,16 a -1,15 |

Los angulos son los medidos en las tramas, no 30 deg exactos. El modulo |a| mediano cambia con la orientacion (8,814 pitch; 8,974 plano; 9,007 roll): es error propio del sensor (offset y escala por eje), y por eso la gravedad calibrada en plano deja un resto de hasta unos 0,16 m/s2 al inclinar.

### 4.3 Movimiento (desplazamiento vertical de unos 20 cm)

| Fichero | Z inicial a final (m) | Z minimo / maximo (m) | Observacion |
|---|---|---|---|
| `mov_lento.txt` (subida en ~3 s) | -0,34 a -0,06 | -0,34 / -0,05 | Z solo sube, `VZ` siempre >= 0; error final de +0,28 m. |
| `mov_rapido.txt` (subida en ~1 s) | -0,06 a -0,14 | -0,14 / +0,14 | Sube unos 0,20 m y baja unos 0,27 m; error final de -0,08 m. |

Explicacion (inferencia, no medida directamente): una bajada lenta de 20 cm en ~3 s requiere unos 0,14 m/s2, comparable con el error que introduce 1 deg de fallo de orientacion (unos 0,17 m/s2), y al mover la placa con la mano el giroscopio marco 4 a 8 deg/s. En movimientos lentos la senal queda dentro del ruido; no se arregla con umbrales.

**Racha interior mas larga por debajo de los umbrales (0,5 m/s2; 3 deg/s):**

| Fichero | Segmento | Racha interior | Muestras a 50 Hz | Frente a 25 muestras |
|---|---|---|---|---|
| `mov_lento.txt` | 5 a 13,5 s | 12 tramas = 1,2 s | ~60 | La supera: reposo falso a mitad del movimiento |
| `mov_rapido.txt` | 4 a 10,4 s | 3 tramas = 0,3 s | ~15 | No la supera |

Una primera medida con segmentos que incluian el reposo final dio 13 y 4 tramas; se sustituyo por la racha interior, que es la relevante.

## 5. Decisiones

| Parametro | Valor | Justificacion |
|---|---|---|
| Umbral de aceleracion | 0,5 m/s2 (sin cambio) | 2,4 veces el peor p99 (0,208) y 2,2 veces el peor max (0,229) medidos; la tolerancia del cero del acelerometro es de hasta +-50 mg (unos 0,49 m/s2) en X e Y segun el fabricante; bajarlo no mejora el movimiento lento. |
| Umbral de giroscopio | 3 deg/s (sin cambio) | El reposo maximo es 0,224 deg/s (mas de 10 veces de margen); el cero del giroscopio varia con la temperatura. |
| Muestras de reposo | 25 = 0,5 s (sin cambio) | Con 25 el movimiento rapido (15 muestras) no se confunde con reposo. En el lento la fase de velocidad casi constante dura ~60 muestras y si se confunde. Subir a ~75 (1,5 s) lo evitaria, pero retrasaria el reposo: error estimado de ~0,18 m (1/2 · 0,16 · 1,5^2) frente a ~0,02 m con 25, y el movimiento lento seguiria sin estimarse bien por el error de orientacion. |

## 6. Limitaciones a declarar

- El movimiento vertical lento no se estima de forma fiable con IMU solo; motiva el uso del barometro.
- Solo se probaron tres orientaciones (plano, pitch 18 deg, roll 26 deg); con otras el resto de gravedad puede ser mayor.
- Estadisticas del giroscopio limitadas por la resolucion de la trama (0,1 deg/s, truncado).
- Pruebas hechas en una sola sesion y una sola placa; movimientos hechos a mano, sin patron reproducible.

## 7. Evidencias

Ficheros en `docs/evidence/phase1/`: `reposo_plano.txt`, `reposo_pitch.txt`, `reposo_roll30.txt`, `mov_lento.txt`, `mov_rapido.txt`, `prueba.txt` (captura de la prueba 1) y `analysis_outputs.txt` con las salidas de `analyze_simu.py`.

## 8. Reproducir

```bash
python tools/capture_simu.py /dev/cu.usbmodem11303 30 fichero.txt
python tools/analyze_simu.py reposo_plano.txt
python tools/analyze_simu.py reposo_pitch.txt --gref 8.974
python tools/analyze_simu.py mov_lento.txt --gref 8.974 --seg 5 13.5 --tol 0.5 --gyro 3
python tools/analyze_simu.py mov_rapido.txt --gref 8.974 --seg 4 10.4 --tol 0.5 --gyro 3
```
