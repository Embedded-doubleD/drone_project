#!/usr/bin/env python3
"""Estadisticas de reposo/movimiento a partir de tramas SIMU capturadas.
Uso: python3 analyze_simu.py captura.txt [captura2.txt ...]
Calcula, con las mismas formulas que motion.c, la aceleracion lineal y el modulo del giroscopio."""
import math, sys

G = 9.80665

def lin_norm(ax, ay, az, roll, pitch, yaw):
    p, t, y = (math.radians(v) for v in (roll, pitch, yaw))
    sp, cp, st, ct, sy, cy = math.sin(p), math.cos(p), math.sin(t), math.cos(t), math.sin(y), math.cos(y)
    awx = ct*cy*ax + (sp*st*cy - cp*sy)*ay + (cp*st*cy + sp*sy)*az
    awy = ct*sy*ax + (sp*st*sy + cp*cy)*ay + (cp*st*sy - sp*cy)*az
    awz = -st*ax + sp*ct*ay + cp*ct*az - G
    return math.sqrt(awx*awx + awy*awy + awz*awz), awz

def pct(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q / 100 * len(v)))]

def analyze(path):
    lin, gyr, zs = [], [], []
    for line in open(path, errors="ignore"):
        line = line.strip()
        if not line.startswith("$SIMU1"):
            continue
        f = line.split("*")[0].split(",")
        if len(f) < 23:
            continue
        z = float(f[10]); roll, pitch, yaw = (float(f[i]) for i in (14, 15, 16))
        ax, ay, az = (float(f[i]) for i in (17, 18, 19))
        gx, gy, gz = (float(f[i]) for i in (20, 21, 22))
        ln, _ = lin_norm(ax, ay, az, roll, pitch, yaw)
        lin.append(ln); gyr.append(math.sqrt(gx*gx + gy*gy + gz*gz)); zs.append(z)
    if not lin:
        print(path, ": sin tramas validas"); return
    print(f"{path}: {len(lin)} tramas, Z de {min(zs):+.2f} a {max(zs):+.2f} m")
    for name, v in (("aceleracion lineal (m/s2)", lin), ("giroscopio (deg/s)", gyr)):
        print(f"  {name}: p50={pct(v,50):.3f} p95={pct(v,95):.3f} p99={pct(v,99):.3f} max={max(v):.3f}")

for p in sys.argv[1:]:
    analyze(p)
