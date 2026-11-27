#!/usr/bin/env python3
"""Guarda en un fichero las tramas SIMU recibidas por el puerto serie.
Uso: python3 capture_simu.py PUERTO SEGUNDOS salida.txt
Ejemplo: python3 capture_simu.py COM5 60 reposo.txt
Requiere: pip install pyserial. Cerrad antes el simulador y el monitor serie:
un puerto solo puede abrirlo un programa a la vez."""
import sys, time
import serial

def main():
    if len(sys.argv) != 4:
        print(__doc__); return 2
    port, seconds, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    ser = serial.serial_for_url(port, baudrate=115200, bytesize=8, parity="N", stopbits=1, timeout=0.5)
    n, t_end = 0, time.time() + seconds
    with open(out, "w") as f:
        while time.time() < t_end:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if line.startswith("$SIMU1"):
                f.write(line + "\n"); n += 1
        f.flush()
    ser.close()
    print(f"{n} tramas guardadas en {out}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
