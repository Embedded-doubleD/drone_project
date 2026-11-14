#include <stdio.h>
#include "telemetry_report.h"
#include "simu_frame.h"

/* Formatea un valor escalado en tmp (con '\0') y devuelve tmp */
static const char *fx(char *tmp, int32_t v, uint8_t int_d, uint8_t frac_d, int signed_field) {
    char *end = simu_fmt_fixed(tmp, v, int_d, frac_d, signed_field);
    *end = '\0';
    return tmp;
}

static const char *state_name(tracker_state_t s) {
    switch (s) {
        case TRACKER_FLYING: return "FLYING";
        case TRACKER_PROBLEM: return "PROBLEM";
        case TRACKER_IDLE:
        default: return "IDLE";
    }
}

int report_format(char *out, size_t out_size, const char *mode_name, const telemetry_t *t) {
    char a[16], b[16], c[16], time_s[32];
    int n = 0, r;

    if (out == NULL || t == NULL || mode_name ==NULL || out_size == 0) { return -1; }

    if (t->gps_utc_hhmmss == 0 && !t->gps_fix) {
        snprintf(time_s, sizeof time_s, "--:--:--");
    } else {
        snprintf(time_s, sizeof time_s, "%02u:%02u:%02u",
                (unsigned)(t->gps_utc_hhmmss / 10000),
                (unsigned)((t->gps_utc_hhmmss / 100) % 100),
                (unsigned)(t->gps_utc_hhmmss % 100));
    }

    r = snprintf(out + n, out_size - n, "MODE:%s\n", mode_name);
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    r = snprintf(out + n, out_size - n, "GPS: fix=%u sats=%02u lat=%s lon=%s alt=%sm time=%s\n",
                 (unsigned)t->gps_fix, (unsigned)t->gps_sats,
                 fx(a, t->lat_e6, 2, 6, 1), fx(b, t->lon_e6, 3, 6, 1),
                 fx(c, t->alt_dm, 5, 1, 1), time_s);
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    r = snprintf(out + n, out_size - n, "IMU: ax=%sm/s2 ay=%sm/s2 az=%sm/s2\n",
                 fx(a, t->ax_cms2, 3, 2, 1), fx(b, t->ay_cms2, 3, 2, 1), fx(c, t->az_cms2, 3, 2, 1));
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    r = snprintf(out + n, out_size - n, "IMU: gx=%sdeg/s gy=%sdeg/s gz=%sdeg/s\n",
                 fx(a, t->gx_ddegs, 4, 1, 1), fx(b, t->gy_ddegs, 4, 1, 1), fx(c, t->gz_ddegs, 4, 1, 1));
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    r = snprintf(out + n, out_size - n, "ATT: roll=%sdeg pitch=%sdeg yaw=%sdeg\n",
                 fx(a, t->roll_cdeg, 3, 2, 1), fx(b, t->pitch_cdeg, 3, 2, 1), fx(c, t->yaw_cdeg, 3, 2, 1));
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    r = snprintf(out + n, out_size - n, "LIGHT: %s%%\nENV: temp=%sC hum=%s%%\nSTATUS: %s\nFLAGS: 0x%04X\n\n",
                 fx(a, t->light_dpct, 3, 1, 0), fx(b, t->temp_dc, 3, 1, 1), fx(c, t->hum_dpct, 3, 1, 0),
                 state_name(t->state), (unsigned)t->flags);
    if (r < 0 || (size_t)r >= out_size - n) { return -1; }
    n += r;

    return n;
}