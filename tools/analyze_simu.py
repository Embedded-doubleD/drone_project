#!/usr/bin/env python3
"""Estadisticas de reposo/movimiento a partir de tramas SIMU capturadas.

Uso: python3 analyze_simu.py captura.txt [opciones]
  --gref V        gravedad de referencia (m/s2). Por defecto, la mediana de |a| de la captura.
                  Para imitar al firmware usad la mediana de |a| del reposo PLANO (la calibrada al arrancar).
  --seg A B       analiza solo de A a B segundos desde el inicio de la captura (reloj TMS).
  --per-second    imprime, por cada segundo, el maximo de aceleracion lineal y giroscopio.
  --tol T --gyro G   con ambos umbrales (m/s2 y deg/s) calcula las rachas de tramas seguidas
                     que el firmware consideraria reposo. Da la racha mas larga TOTAL y la racha
                     INTERIOR mas larga (la que queda rodeada de movimiento, sin contar el reposo
                     del principio ni del final del tramo).

Tambien informa de huecos en la captura (tramas que faltan segun el reloj de la placa) y de la
orientacion mediana (roll, pitch, yaw)."""
import math, sys

G0 = 9.80665

def lin_norm(ax, ay, az, roll, pitch, yaw, g):
    p, t, y = (math.radians(v) for v in (roll, pitch, yaw))
    sp, cp, st, ct, sy, cy = math.sin(p), math.cos(p), math.sin(t), math.cos(t), math.sin(y), math.cos(y)
    awx = ct*cy*ax + (sp*st*cy - cp*sy)*ay + (cp*st*cy + sp*sy)*az
    awy = ct*sy*ax + (sp*st*sy + cp*cy)*ay + (cp*st*sy - sp*cy)*az
    awz = -st*ax + sp*ct*ay + cp*ct*az - g
    return math.sqrt(awx*awx + awy*awy + awz*awz)

def pct(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q / 100 * len(v)))]

def stats(v):
    return f"p50={pct(v,50):.3f} p95={pct(v,95):.3f} p99={pct(v,99):.3f} max={max(v):.3f}"

def load(path):
    rows = []
    for line in open(path, errors="ignore"):
        line = line.strip()
        if not line.startswith("$SIMU1"):
            continue
        f = line.split("*")[0].split(",")
        if len(f) < 23:
            continue
        rows.append(dict(tms=int(f[2]), z=float(f[10]), vz=float(f[13]),
                         att=tuple(float(f[i]) for i in (14, 15, 16)),
                         a=tuple(float(f[i]) for i in (17, 18, 19)),
                         g=tuple(float(f[i]) for i in (20, 21, 22))))
    return rows

def opt(args, name, n):
    if name not in args:
        return None
    i = args.index(name); vals = [float(x) for x in args[i + 1:i + 1 + n]]
    del args[i:i + 1 + n]
    return vals

def runs(flags):
    """Devuelve (inicio, fin) de cada racha de True (fin exclusivo)."""
    out, start = [], None
    for i, f in enumerate(flags + [False]):
        if f and start is None:
            start = i
        if not f and start is not None:
            out.append((start, i)); start = None
    return out

def main():
    args = sys.argv[1:]
    gref = opt(args, "--gref", 1)
    seg = opt(args, "--seg", 2)
    tol = opt(args, "--tol", 1)
    gyr = opt(args, "--gyro", 1)
    per_second = "--per-second" in args
    if per_second:
        args.remove("--per-second")
    for path in args:
        rows = load(path)
        if not rows:
            print(path, ": sin tramas validas"); continue
        t0 = rows[0]["tms"]
        for r in rows:
            r["t"] = (r["tms"] - t0) / 1000.0
            r["an"] = math.sqrt(sum(c*c for c in r["a"]))
            r["gn"] = math.sqrt(sum(c*c for c in r["g"]))
        expected = int(round(rows[-1]["t"] / 0.1)) + 1
        gaps = [b["tms"] - a["tms"] for a, b in zip(rows, rows[1:])]
        big = sum(1 for g in gaps if g > 150)
        print(f"{path}: {len(rows)} tramas de ~{expected} esperadas por TMS; "
              f"{big} saltos > 150 ms, el mayor de {max(gaps) if gaps else 0} ms")
        g_est = pct([r["an"] for r in rows], 50) if gref is None else gref[0]
        for r in rows:
            r["l0"] = lin_norm(*r["a"], *r["att"], G0)
            r["l1"] = lin_norm(*r["a"], *r["att"], g_est)
        if seg:
            rows = [r for r in rows if seg[0] <= r["t"] <= seg[1]]
            if not rows:
                print("  segmento vacio"); continue
        print(f"  tramo analizado: t={rows[0]['t']:.1f} a {rows[-1]['t']:.1f} s ({len(rows)} tramas)")
        print(f"  orientacion mediana: roll={pct([r['att'][0] for r in rows],50):+.1f} "
              f"pitch={pct([r['att'][1] for r in rows],50):+.1f} yaw={pct([r['att'][2] for r in rows],50):+.1f} deg")
        print(f"  Z de {min(r['z'] for r in rows):+.2f} a {max(r['z'] for r in rows):+.2f} m;"
              f" VZ de {min(r['vz'] for r in rows):+.2f} a {max(r['vz'] for r in rows):+.2f} m/s")
        print(f"  modulo |a| (m/s2): {stats([r['an'] for r in rows])}")
        print(f"  aceleracion lineal con g=9.80665: {stats([r['l0'] for r in rows])}")
        print(f"  aceleracion lineal con g={g_est:.3f}: {stats([r['l1'] for r in rows])}")
        print(f"  giroscopio (deg/s): {stats([r['gn'] for r in rows])}")
        if per_second:
            print("  seg | lin max | gyro max |   Z     |  VZ")
            for s in range(int(rows[0]["t"]), int(rows[-1]["t"]) + 1):
                b = [r for r in rows if s <= r["t"] < s + 1]
                if b:
                    print(f"  {s:3d} | {max(r['l1'] for r in b):7.3f} | {max(r['gn'] for r in b):8.3f} |"
                          f" {b[-1]['z']:+8.2f} | {b[-1]['vz']:+6.2f}")
        if tol and gyr:
            still = [(r["l1"] < tol[0] and r["gn"] < gyr[0]) for r in rows]
            rs = runs(still)
            total = max((e - s for s, e in rs), default=0)
            inner = max((e - s for s, e in rs if s > 0 and e < len(rows)), default=0)
            print(f"  umbrales ({tol[0]} m/s2, {gyr[0]} deg/s):")
            print(f"    racha mas larga TOTAL:    {total} tramas = {total*0.1:.1f} s (~{total*5} muestras a 50 Hz)")
            print(f"    racha INTERIOR mas larga: {inner} tramas = {inner*0.1:.1f} s (~{inner*5} muestras a 50 Hz)"
                  "  <- la que importa dentro del movimiento")

main()