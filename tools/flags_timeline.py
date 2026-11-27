#!/usr/bin/env python3
"""Muestra cuando cambian los FLAGS en una captura de tramas SIMU.
Uso: python3 flags_timeline.py captura.txt
Sirve como evidencia de las pruebas de fallo: cuando se activa y se borra cada bit."""
import sys

BITS = {0x0001: "IMU", 0x0002: "GPS", 0x0004: "LDR", 0x0008: "ENV",
        0x0010: "IMPACTO", 0x0020: "CAL_PENDIENTE", 0x0040: "SOLO_INERCIAL", 0x0080: "YAW_NO_FIABLE"}

def names(v):
    return ",".join(n for b, n in BITS.items() if v & b) or "-"

def main(path):
    prev, first_tms, n = None, None, 0
    for line in open(path, errors="ignore"):
        line = line.strip()
        if not line.startswith("$SIMU1"):
            continue
        f = line.split("*")[0].split(",")
        if len(f) < 28:
            continue
        n += 1
        seq, tms, flags = int(f[1]), int(f[2]), int(f[27], 16)
        first_tms = tms if first_tms is None else first_tms
        if flags != prev:
            if prev is None:
                print(f"t={(tms-first_tms)/1000:7.1f} s  SEQ {seq:05d}  FLAGS 0x{flags:04X}  [{names(flags)}]  (inicio)")
            else:
                on, off = flags & ~prev, prev & ~flags
                print(f"t={(tms-first_tms)/1000:7.1f} s  SEQ {seq:05d}  FLAGS 0x{flags:04X}  activa: {names(on)}  borra: {names(off)}")
            prev = flags
    print(f"{n} tramas leidas")

main(sys.argv[1])