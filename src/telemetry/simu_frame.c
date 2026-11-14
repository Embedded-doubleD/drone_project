#include <string.h>
#include "simu_frame.h"

char *simu_fmt_uint(char *p, uint64_t val, uint8_t digits)
{
    for (int i = digits - 1; i >= 0; i--) {
        p[i] = (char)('0' + (val % 10));
        val /= 10;
    }
    return p + digits;
}

/* v = valor * 10^frac_d. Satura al ancho del campo para no romper el formato. */
char *simu_fmt_fixed(char *p, int32_t v, uint8_t int_d, uint8_t frac_d, int signed_field)
{
    uint64_t scale = 1, limit = 1;
    for (uint8_t i = 0; i < frac_d; i++) { scale *= 10; }
    for (uint8_t i = 0; i < int_d + frac_d; i++) { limit *= 10; }

    int neg = (v < 0);
    uint64_t a = neg ? (uint64_t)(-(int64_t)v) : (uint64_t)v;
    if (a >= limit) { a = limit - 1; }
    if (!signed_field && neg) { a = 0; neg = 0; }   /* campos sin signo: nunca negativos */

    if (signed_field) { *p++ = neg ? '-' : '+'; }
    p = simu_fmt_uint(p, a / scale, int_d);
    *p++ = '.';
    return simu_fmt_uint(p, a % scale, frac_d);
}

static char *put_hex(char *p, uint32_t val, uint8_t digits)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        p[i] = hex[val & 0xF];
        val >>= 4;
    }
    return p + digits;
}

static const char *state_text(tracker_state_t s)
{
    switch (s) {
    case TRACKER_FLYING:  return "FLYING__";
    case TRACKER_PROBLEM: return "PROBLEM_";
    case TRACKER_IDLE:
    default:              return "IDLE____";
    }
}

int simu_frame_build(char *out, size_t out_size, uint16_t seq,
                     uint32_t tms_ms, const telemetry_t *t)
{
    if (out == NULL || t == NULL || out_size < SIMU_FRAME_BUF) { return -1; }

    char *p = out;
    uint8_t sats = (t->gps_sats > 99) ? 99 : t->gps_sats;

    *p++ = '$';
    memcpy(p, "SIMU1", 5);  p += 5;                            *p++ = ',';
    p = simu_fmt_uint(p, seq, 5);                              *p++ = ',';
    p = simu_fmt_uint(p, tms_ms, 10);                          *p++ = ',';
    *p++ = t->gps_fix ? '1' : '0';                             *p++ = ',';
    p = simu_fmt_uint(p, sats, 2);                             *p++ = ',';
    p = simu_fmt_fixed(p, t->lat_e6,    2, 6, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->lon_e6,    3, 6, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->alt_dm,    5, 1, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->x_cm,      4, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->y_cm,      4, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->z_cm,      4, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->vx_cms,    3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->vy_cms,    3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->vz_cms,    3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->roll_cdeg, 3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->pitch_cdeg,3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->yaw_cdeg,  3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->ax_cms2,   3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->ay_cms2,   3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->az_cms2,   3, 2, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->gx_ddegs,  4, 1, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->gy_ddegs,  4, 1, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->gz_ddegs,  4, 1, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->light_dpct,3, 1, 0);              *p++ = ',';
    p = simu_fmt_fixed(p, t->temp_dc,   3, 1, 1);              *p++ = ',';
    p = simu_fmt_fixed(p, t->hum_dpct,  3, 1, 0);              *p++ = ',';
    memcpy(p, state_text(t->state), 8);  p += 8;               *p++ = ',';
    p = put_hex(p, t->flags, 4);

    uint8_t cs = 0;
    for (const char *c = out + 1; c < p; c++) { cs ^= (uint8_t)*c; }

    *p++ = '*';
    p = put_hex(p, cs, 2);
    *p++ = '\r';
    *p++ = '\n';
    *p = '\0';

    return (int)(p - out);
}
