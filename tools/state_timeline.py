#!/usr/bin/env python3
"""Linea de tiempo conjunta de STATE y FLAGS en una captura SIMU, con tiempos del reloj de la placa (TMS).

Uso: python3 state_timeline.py captura.txt [mascara_hex]
  mascara_hex: bit de FLAGS cuya relacion con el estado se quiere medir (por defecto 0x0001 = IMU invalida).

Al final calcula, si existen en la captura:
  - retardo entre activarse el bit y entrar en PROBLEM,
  - retardo entre borrarse el bit y salir de PROBLEM.
"""
import sys

NAMES = {0x0001: "IMU", 0x0002: "GPS", 0x0004: "LDR", 0x0008: "ENV", 0x0010: "IMPACTO",
         0x0020: "CAL_PENDIENTE", 0x0040: "SOLO_INERCIAL", 0x0080: "YAW_NO_FIABLE"}

def names(flags):
    return ",".join(n for b, n in NAMES.items() if flags & b) or "-"

def load(path):
    rows = []
    for line in open(path, errors="ignore"):
        line = line.strip()
        if not line.startswith("$SIMU1"):
            continue
        f = line.split("*")[0].split(",")
        if len(f) < 28:
            continue
        try:
            rows.append((int(f[2]), int(f[1]), f[26].strip("_"), int(f[27], 16)))
        except ValueError:
            continue
    return rows

def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    mask = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x0001
    rows = load(sys.argv[1])
    if not rows:
        sys.exit("sin tramas validas")
    t0 = rows[0][0]
    prev = None
    ev = {}
    for tms, seq, state, flags in rows:
        t = (tms - t0) / 1000.0
        if prev is None:
            print(f"t={t:7.1f} s  SEQ {seq:05d}  STATE {state:8s}  FLAGS 0x{flags:04X} [{names(flags)}]  (inicio)")
        elif (state, flags) != prev:
            ch = []
            if state != prev[1 - 1]:
                ch.append(f"estado {prev[0]} -> {state}")
            if flags != prev[1]:
                on, off = flags & ~prev[1], prev[1] & ~flags
                ch.append(f"flags +[{names(on)}] -[{names(off)}]")
            print(f"t={t:7.1f} s  SEQ {seq:05d}  STATE {state:8s}  FLAGS 0x{flags:04X}  " + "; ".join(ch))
            if flags & mask and not prev[1] & mask:
                ev.setdefault("set", t)
            if prev[1] & mask and not flags & mask:
                ev.setdefault("clr", t)
            if state == "PROBLEM" and prev[0] != "PROBLEM":
                ev.setdefault("pin", t)
            if prev[0] == "PROBLEM" and state != "PROBLEM":
                ev.setdefault("pout", t)
        prev = (state, flags)
    print(f"{len(rows)} tramas leidas ({(rows[-1][0]-t0)/1000:.1f} s)")
    if "set" in ev and "pin" in ev:
        print(f"Bit activado -> PROBLEM: {ev['pin'] - ev['set']:+.1f} s")
    if "clr" in ev and "pout" in ev:
        print(f"Bit borrado -> sale de PROBLEM: {ev['pout'] - ev['clr']:+.1f} s")

main()
