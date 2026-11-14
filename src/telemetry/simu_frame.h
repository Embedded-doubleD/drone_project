#ifndef SIMU_FRAME_H
#define SIMU_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include "telemetry_data.h"

#define SIMU_FRAME_LEN 221              /* incluye \r\n */
#define SIMU_FRAME_BUF (SIMU_FRAME_LEN + 1)

/* Devuelve SIMU_FRAME_LEN si todo va bien, o un valor negativo si el buffer es corto. */
int simu_frame_build(char *out, size_t out_size, uint16_t seq,
                     uint32_t tms_ms, const telemetry_t *t);

/* Ayudantes de formato de ancho fijo (también los usa el informe de texto).
Devuelven el puntero situado tras el último carcater escrito: no escriben '\0'. */
char *simu_fmt_uint(char *p, uint64_t val, uint8_t digits);
/* v = valor * 10^frac_d; satura al ancho; signed_field = 1 antepone '+' o '-' */
char *simu_fmt_fixed(char *p, int32_t v, uint8_t int_d, uint8_t frac_d, int signed_field);

#endif
