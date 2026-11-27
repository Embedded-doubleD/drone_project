# Medicion del uso de pila de los hilos

Fecha: 27-nov-2026 · Rama: `feat/sensor-robustness` · Placa: STM32WL55JC (Nucleo) · Herramienta: Zephyr Thread Analyzer (solo en build de medida)

## 1. Metodo

- Build de medida con el Thread Analyzer activo (volcado cada 5 s por consola) y nombres de hilo activados. **No forma parte del firmware final**: imprime texto y, en modo SIMU, el volcado se mezcla con las tramas e incumpliria SM6.
- El analizador informa del **maximo de pila usado desde el arranque** (marca de agua); la medida valida es la ultima de cada arranque.
- Regla de dimensionado: `tamano >= maximo medido x 1,25`.
- La cifra de la IMU coincide con la linea `IMU STACK: unused=360 bytes` del bloque de estadisticas del propio firmware.
- Escenarios cubiertos: SIMU, TEST, NORMAL (con el bloque de estadisticas de 30 s) y desconexion y reconexion de la IMU (el estado paso a PROBLEM y volvio a IDLE).

## 2. Resultados (maximo usado, en bytes)

| Hilo | Tamano actual | Maximo medido | Libre | Minimo por regla | Decision |
|---|---|---|---|---|---|
| `imu_thread_id` | 1024 | 664 (sin cambio tras desconectar y reconectar) | 35 % | 830 | Se mantiene |
| `out_tid` | 2048 | 704 | 66 % | 880 | Se mantiene (reducir a 1024 es opcional) |
| `gps_tid` | 2048 | 488 | 76 % | 610 | Se mantiene: el camino con fix GPS no se pudo medir |
| `logging` | 768 | 648 | 16 % | 810 | **Sube a 1024** |
| `idle` | 256 | 208 | 19 % | 260 | **Sube a 320** |
| `main` | 1024 | 480 | 53 % | 600 | Se mantiene (reducir a 768 es opcional) |
| ISR0 | 2048 | 328 | 84 % | 410 | Se mantiene |
| `thread_analyzer` | 1024 | 520 | - | - | Solo en el build de medida |

Coste neto de los cambios: +320 B de RAM (opcionales: -1024 B en `out_tid`, -256 B en `main`).

## 3. Observaciones

- La marca de agua de `logging` crecio con la actividad: 520 B, 612 B y 648 B en las sucesivas medidas, la ultima tras las desconexiones de la IMU. Se dimensiona con el peor caso observado y se repite la medida tras el cambio.
- Cargas de CPU (aprox.): IMU 4 %, salida 2 %, logging 2-3 %, idle 89-93 %. La aplicacion deja mas del 85 % del tiempo libre (GR1, GR6).
- `idle`: 208 B en todos los arranques, camino fijo; es un hilo del sistema.
- `out_tid` marca 704 B tambien en NORMAL con el bloque de estadisticas impreso.
- Durante la recuperacion de la IMU, el informe mostro `STATUS: PROBLEM` con `FLAGS=0x00C2` (sin el bit de IMU invalida): el sensor ya habia vuelto a dar datos y el estado se mantenia en PROBLEM durante el periodo de recuperacion del tracker.

## 4. Limitaciones

- No medido el camino de GPS con fix (sin acceso a cielo abierto). Por eso `gps_tid` conserva 2048 B.
- La marca de agua solo vale para los escenarios probados.
- Una sola placa y una sola version del firmware.
- No se midio el tiempo de entrada y salida de PROBLEM con reloj de la placa (pendiente con una captura SIMU y `flags_timeline.py` en el build final).

## 5. Cambios de configuracion (`confs/prj_nucleo_wl55jc.conf`)

```
CONFIG_LOG_PROCESS_THREAD_STACK_SIZE=1024
CONFIG_IDLE_STACK_SIZE=320
```

Se verifica con una ultima medida con el analizador activo (tras repetir varias desconexiones de la IMU) y, despues, se compila sin el.
