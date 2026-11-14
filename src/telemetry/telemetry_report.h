#ifndef TELEMETRY_REPORT_H
#define TELEMETRY_REPORT_H

#include <stddef.h>
#include "telemetry_data.h"

#define REPORT_BUF 384 /* Peor caso con 320 bytes (todos los campos al máximo con signo, estado PROBLEM, modo NORMAL, banderas 0xFFFF) */

/* Informe legible para los modos TEST y NORMAL. mode_name: "TEST" o "NORMAL".
Devuelve el número de caracteres escritos (sin contar el '\0), o <0 si no cabe. */
int report_format(char *out, size_t out_size, const char *mode_name, const telemetry_t *t);

#endif /* TELEMETRY_REPORT_H */