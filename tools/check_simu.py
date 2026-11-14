#!/usr/bin/env python3
"""Valida tramas SIMU1 contra la especificacion del enunciado.

Uso:
  python check_simu.py --selftest
  python check_simu.py --file captura.txt [--skip-lines N]
  python check_simu.py --port /dev/cu.usbmodemXXXX --seconds 20 [--warmup 1.5]

--warmup S : segundos iniciales (solo con --port) cuyos errores NO se cuentan.
             Se muestran igualmente como "arranque (descartado)". Con 0 se cuenta todo.
"""
import argparse
import re
import sys
import time

FIELDS = [
    ("MARK", r"\$SIMU1"), ("SEQ", r"\d{5}"), ("TMS", r"\d{10}"), ("GF", r"[01]"),
    ("SAT", r"\d{2}"), ("LAT", r"[+-]\d{2}\.\d{6}"), ("LON", r"[+-]\d{3}\.\d{6}"),
    ("ALT", r"[+-]\d{5}\.\d"), ("X", r"[+-]\d{4}\.\d{2}"), ("Y", r"[+-]\d{4}\.\d{2}"),
    ("Z", r"[+-]\d{4}\.\d{2}"), ("VX", r"[+-]\d{3}\.\d{2}"), ("VY", r"[+-]\d{3}\.\d{2}"),
    ("VZ", r"[+-]\d{3}\.\d{2}"), ("ROLL", r"[+-]\d{3}\.\d{2}"), ("PITCH", r"[+-]\d{3}\.\d{2}"),
    ("YAW", r"[+-]\d{3}\.\d{2}"), ("AX", r"[+-]\d{3}\.\d{2}"), ("AY", r"[+-]\d{3}\.\d{2}"),
    ("AZ", r"[+-]\d{3}\.\d{2}"), ("GX", r"[+-]\d{4}\.\d"), ("GY", r"[+-]\d{4}\.\d"),
    ("GZ", r"[+-]\d{4}\.\d"), ("LIGHT", r"\d{3}\.\d"), ("TEMP", r"[+-]\d{3}\.\d"),
    ("HUM", r"\d{3}\.\d"), ("STATE", r"(?:IDLE____|FLYING__|PROBLEM_)"), ("FLAGS", r"[0-9A-F]{4}"),
]
FRAME_LEN = 221  # incluye \r\n


def check_frame(raw: bytes):
    """Devuelve (lista_de_errores, campos|None)."""
    errors = []
    if len(raw) != FRAME_LEN:
        errors.append(f"longitud {len(raw)} (esperada {FRAME_LEN} con \\r\\n)")
    if not raw.endswith(b"\r\n"):
        errors.append("no termina en \\r\\n")
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError:
        return errors + ["caracteres no ASCII"], None
    text = text.rstrip("\r\n")
    if not text.startswith("$SIMU"):
        return errors + ["texto que no es una trama SIMU"], None
    if "*" not in text:
        return errors + ["sin '*' de checksum"], None
    body, cs_txt = text[1:].rsplit("*", 1)
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    if not re.fullmatch(r"[0-9A-F]{2}", cs_txt):
        errors.append(f"checksum mal formado: {cs_txt!r}")
    elif int(cs_txt, 16) != cs:
        errors.append(f"checksum {cs_txt} != calculado {cs:02X}")
    parts = text.split("*")[0].split(",")
    if len(parts) != len(FIELDS):
        return errors + [f"{len(parts)} campos (esperados {len(FIELDS)})"], None
    values = {}
    for (name, rx), val in zip(FIELDS, parts):
        if not re.fullmatch(rx, val):
            errors.append(f"campo {name} invalido: {val!r}")
        values[name] = val
    return errors, values


def check_stream(lines, stamps=None, warmup_s=0.0, skip_lines=0):
    ok = bad = ignored = seq_jumps = 0
    prev_seq = prev_tms = None
    first_seq = last_seq = None
    deltas = []
    t0 = stamps[0] if stamps else None
    for i, raw in enumerate(lines):
        in_warmup = (i < skip_lines) or (stamps is not None and warmup_s > 0
                                          and stamps[i] - t0 < warmup_s)
        errors, vals = check_frame(raw)
        if in_warmup:
            if errors:
                ignored += 1
                print(f"[{i}] arranque (descartado): {'; '.join(errors)}")
            continue
        if errors:
            bad += 1
            print(f"[{i}] ERROR: {'; '.join(errors)}")
            prev_seq = prev_tms = None   # tras un error no se compara con la trama anterior
            continue
        ok += 1
        seq, tms = int(vals["SEQ"]), int(vals["TMS"])
        if first_seq is None:
            first_seq = seq
        last_seq = seq
        if prev_seq is not None:
            if seq != (prev_seq + 1) % 65536:
                seq_jumps += 1
                print(f"[{i}] salto de secuencia: {prev_seq} -> {seq}")
            else:
                deltas.append(tms - prev_tms)
        prev_seq, prev_tms = seq, tms
    print(f"Tramas validas: {ok}  con errores: {bad}  saltos de secuencia: {seq_jumps}"
          f"  (errores descartados en el arranque: {ignored})")
    if first_seq is not None:
        print(f"SEQ primera valida: {first_seq}  ultima: {last_seq}")
    if deltas:
        mean = sum(deltas) / len(deltas)
        off = sum(1 for d in deltas if abs(d - 100) > 5)
        print(f"Periodo segun TMS (reloj de la placa): min {min(deltas)} ms, max {max(deltas)} ms, "
              f"media {mean:.1f} ms sobre {len(deltas)} intervalos; fuera de 100+-5 ms: {off}")
    if stamps and len(stamps) > 1:
        dt = (stamps[-1] - stamps[0]) / (len(stamps) - 1)
        print(f"Periodo medio medido en el PC: {dt * 1000:.1f} ms (depende del USB y del PC)")
    return bad == 0 and seq_jumps == 0 and ok > 0


def split_lines(data: bytes):
    return [c + b"\n" for c in data.split(b"\n") if c]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--file")
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--warmup", type=float, default=1.5)
    ap.add_argument("--skip-lines", type=int, default=0)
    a = ap.parse_args()

    if a.selftest:
        ex = (b"$SIMU1,00152,0000123456,1,08,+40.389462,-003.627416,+00628.4,+0012.34,-0003.20,"
              b"+0015.00,+000.50,-000.10,+001.20,+002.40,-001.10,+045.00,-000.12,+000.04,+009.81,"
              b"+0000.5,-0001.2,+0000.1,073.5,+024.6,045.2,FLYING__,0000*3F\r\n")
        sys.exit(0 if check_stream([ex]) else 1)

    if a.file:
        with open(a.file, "rb") as f:
            lines = split_lines(f.read())
        sys.exit(0 if check_stream(lines, skip_lines=a.skip_lines) else 1)

    if a.port:
        import serial  # pip install pyserial
        ser = serial.Serial(a.port, a.baud, timeout=1)
        ser.reset_input_buffer()
        lines, stamps = [], []
        t_end = time.time() + a.seconds
        buf = b""
        while time.time() < t_end:
            buf += ser.read(ser.in_waiting or 1)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                lines.append(line + b"\n")
                stamps.append(time.time())
        ser.close()
        sys.exit(0 if check_stream(lines, stamps, warmup_s=a.warmup) else 1)

    ap.print_help()


if __name__ == "__main__":
    main()