#!/usr/bin/env python3
"""Valida tramas SIMU (protocolo SIMU1) contra la especificacion del enunciado.

Uso:
  python check_simu.py --selftest                  # comprueba el ejemplo del enunciado
  python check_simu.py --file captura.txt          # valida una captura guardada del terminal
  python check_simu.py --port COM5 --seconds 20    # lee del Virtual COM (requiere: pip install pyserial)
"""
import argparse
import re
import sys
import time

FIELDS = [
    ("MARK",  r"\$SIMU1"),
    ("SEQ",   r"\d{5}"),
    ("TMS",   r"\d{10}"),
    ("GF",    r"[01]"),
    ("SAT",   r"\d{2}"),
    ("LAT",   r"[+-]\d{2}\.\d{6}"),
    ("LON",   r"[+-]\d{3}\.\d{6}"),
    ("ALT",   r"[+-]\d{5}\.\d"),
    ("X",     r"[+-]\d{4}\.\d{2}"),
    ("Y",     r"[+-]\d{4}\.\d{2}"),
    ("Z",     r"[+-]\d{4}\.\d{2}"),
    ("VX",    r"[+-]\d{3}\.\d{2}"),
    ("VY",    r"[+-]\d{3}\.\d{2}"),
    ("VZ",    r"[+-]\d{3}\.\d{2}"),
    ("ROLL",  r"[+-]\d{3}\.\d{2}"),
    ("PITCH", r"[+-]\d{3}\.\d{2}"),
    ("YAW",   r"[+-]\d{3}\.\d{2}"),
    ("AX",    r"[+-]\d{3}\.\d{2}"),
    ("AY",    r"[+-]\d{3}\.\d{2}"),
    ("AZ",    r"[+-]\d{3}\.\d{2}"),
    ("GX",    r"[+-]\d{4}\.\d"),
    ("GY",    r"[+-]\d{4}\.\d"),
    ("GZ",    r"[+-]\d{4}\.\d"),
    ("LIGHT", r"\d{3}\.\d"),
    ("TEMP",  r"[+-]\d{3}\.\d"),
    ("HUM",   r"\d{3}\.\d"),
    ("STATE", r"(?:IDLE____|FLYING__|PROBLEM_)"),
    ("FLAGS", r"[0-9A-F]{4}"),
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
    if "*" not in text or not text.startswith("$"):
        return errors + ["sin '$' inicial o '*' de checksum"], None
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


def check_stream(lines, timestamps=None):
    ok = bad = 0
    prev_seq = None
    seq_jumps = 0
    for i, raw in enumerate(lines):
        errors, vals = check_frame(raw)
        if errors:
            bad += 1
            print(f"[{i}] ERROR: {'; '.join(errors)}")
        else:
            ok += 1
            seq = int(vals["SEQ"])
            if prev_seq is not None and seq != (prev_seq + 1) % 65536:
                seq_jumps += 1
                print(f"[{i}] salto de secuencia: {prev_seq} -> {seq}")
            prev_seq = seq
    print(f"Tramas validas: {ok}  con errores: {bad}  saltos de secuencia: {seq_jumps}")
    if timestamps and len(timestamps) > 1:
        dt = (timestamps[-1] - timestamps[0]) / (len(timestamps) - 1)
        print(f"Periodo medio entre tramas: {dt * 1000:.1f} ms  (nominal 100 ms)")
    return bad == 0 and ok > 0


def split_lines(data: bytes):
    out = []
    for chunk in data.split(b"\n"):
        if chunk:
            out.append(chunk + b"\n")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--file")
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=10.0)
    a = ap.parse_args()

    if a.selftest:
        ex = (b"$SIMU1,00152,0000123456,1,08,+40.389462,-003.627416,+00628.4,+0012.34,-0003.20,"
              b"+0015.00,+000.50,-000.10,+001.20,+002.40,-001.10,+045.00,-000.12,+000.04,+009.81,"
              b"+0000.5,-0001.2,+0000.1,073.5,+024.6,045.2,FLYING__,0000*3F\r\n")
        sys.exit(0 if check_stream([ex]) else 1)

    if a.file:
        with open(a.file, "rb") as f:
            data = f.read()
        lines = [l for l in split_lines(data) if l.startswith(b"$SIMU")]
        sys.exit(0 if check_stream(lines) else 1)

    if a.port:
        import serial  # pip install pyserial
        ser = serial.Serial(a.port, a.baud, timeout=1)
        lines, stamps = [], []
        t_end = time.time() + a.seconds
        buf = b""
        while time.time() < t_end:
            buf += ser.read(ser.in_waiting or 1)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line += b"\n"
                if line.startswith(b"$SIMU"):
                    lines.append(line)
                    stamps.append(time.time())
        ser.close()
        sys.exit(0 if check_stream(lines, stamps) else 1)

    ap.print_help()


if __name__ == "__main__":
    main()