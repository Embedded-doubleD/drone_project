#include <ctype.h>
#include <string.h>
#include "nmea.h"

#define NMEA_MAX_BODY   96
#define NMEA_MAX_FIELDS 20

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    return -1;
}

bool nmea_checksum_ok(const char *line)
{
    if (line == NULL || line[0] != '$') { return false; }

    const char *star = strchr(line, '*');
    if (star == NULL) { return false; }

    int hi = hex_val(star[1]);
    int lo = (hi >= 0) ? hex_val(star[2]) : -1;
    if (hi < 0 || lo < 0) { return false; }

    uint8_t cs = 0;
    for (const char *c = line + 1; c < star; c++) { cs ^= (uint8_t)*c; }

    return cs == (uint8_t)((hi << 4) | lo);
}

/* Separa por comas CONSERVANDO los campos vacios (strtok los colapsaria). */
static int split_fields(char *s, char *f[], int max)
{
    int n = 0;
    f[n++] = s;
    for (; *s != '\0'; s++) {
        if (*s == ',') {
            *s = '\0';
            if (n >= max) { return -1; }
            f[n++] = s + 1;
        }
    }
    return n;
}

static bool all_digits(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) { return false; }
    }
    return true;
}

static bool parse_uint(const char *s, uint32_t max, uint32_t *out)
{
    size_t n = strlen(s);
    if (n == 0 || n > 9 || !all_digits(s, n)) { return false; }
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) { v = v * 10 + (uint32_t)(s[i] - '0'); }
    if (v > max) { return false; }
    *out = v;
    return true;
}

/* hhmmss[.sss] */
static bool parse_time(const char *s, uint32_t *out)
{
    const char *dot = strchr(s, '.');
    size_t n = dot ? (size_t)(dot - s) : strlen(s);
    if (n != 6 || !all_digits(s, 6)) { return false; }
    if (dot && !all_digits(dot + 1, strlen(dot + 1))) { return false; }

    uint32_t hh = (uint32_t)((s[0] - '0') * 10 + (s[1] - '0'));
    uint32_t mm = (uint32_t)((s[2] - '0') * 10 + (s[3] - '0'));
    uint32_t ss = (uint32_t)((s[4] - '0') * 10 + (s[5] - '0'));
    if (hh > 23 || mm > 59 || ss > 60) { return false; }

    *out = hh * 10000 + mm * 100 + ss;
    return true;
}

/* ddmm.mmmm (lat, int_digits = 4) o dddmm.mmmm (lon, int_digits = 5) -> grados * 1e6 */
static bool parse_coord(const char *s, const char *hemi, size_t int_digits,
                        int32_t deg_max, char pos, char neg, int32_t *out)
{
    const char *dot = strchr(s, '.');
    size_t n_int = dot ? (size_t)(dot - s) : strlen(s);
    if (n_int != int_digits || !all_digits(s, n_int)) { return false; }

    size_t n_frac = dot ? strlen(dot + 1) : 0;
    if (dot && !all_digits(dot + 1, n_frac)) { return false; }
    if (hemi[0] == '\0' || hemi[1] != '\0') { return false; }
    if (hemi[0] != pos && hemi[0] != neg) { return false; }

    int64_t deg = 0;
    for (size_t i = 0; i < n_int - 2; i++) { deg = deg * 10 + (s[i] - '0'); }
    int64_t min_int = (s[n_int - 2] - '0') * 10 + (s[n_int - 1] - '0');
    if (min_int >= 60 || deg > deg_max) { return false; }

    /* fraccion de minuto en millonesimas (se descartan digitos a partir del 7.o) */
    int64_t frac = 0;
    for (size_t i = 0; i < 6; i++) {
        frac = frac * 10 + ((dot && i < n_frac) ? (dot[1 + i] - '0') : 0);
    }

    int64_t min_micro = min_int * 1000000 + frac;
    int64_t e6 = deg * 1000000 + (min_micro + 30) / 60;   /* redondeo al mas cercano */
    if (e6 > (int64_t)deg_max * 1000000) { return false; }

    *out = (int32_t)((hemi[0] == neg) ? -e6 : e6);
    return true;
}

/* [-]iii[.d...] -> valor * 10 (se trunca a 1 decimal) */
static bool parse_tenths(const char *s, int32_t *out)
{
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    else if (*s == '+') { s++; }

    const char *dot = strchr(s, '.');
    size_t n_int = dot ? (size_t)(dot - s) : strlen(s);
    if (n_int == 0 || n_int > 7 || !all_digits(s, n_int)) { return false; }
    if (dot && !all_digits(dot + 1, strlen(dot + 1))) { return false; }

    int32_t v = 0;
    for (size_t i = 0; i < n_int; i++) { v = v * 10 + (s[i] - '0'); }
    v *= 10;
    if (dot && dot[1] != '\0') { v += dot[1] - '0'; }

    *out = neg ? -v : v;
    return true;
}

int nmea_parse_gga(const char *line, nmea_gga_t *out)
{
    if (line == NULL || out == NULL) { return NMEA_ERR_ARG; }
    memset(out, 0, sizeof(*out));

    if (!nmea_checksum_ok(line)) { return NMEA_ERR_FORMAT; }

    const char *star = strchr(line, '*');
    size_t len = (size_t)(star - (line + 1));
    if (len >= NMEA_MAX_BODY) { return NMEA_ERR_FORMAT; }

    char buf[NMEA_MAX_BODY];
    memcpy(buf, line + 1, len);
    buf[len] = '\0';

    char *f[NMEA_MAX_FIELDS];
    int n = split_fields(buf, f, NMEA_MAX_FIELDS);
    if (n < 0) { return NMEA_ERR_FORMAT; }

    if (strlen(f[0]) != 5 || strcmp(f[0] + 2, "GGA") != 0) { return NMEA_ERR_NOT_GGA; }
    if (n < 10) { return NMEA_ERR_MALFORMED; }

    /* campos: 1 hora, 2 lat, 3 N/S, 4 lon, 5 E/W, 6 calidad, 7 satelites, 9 altitud */
    uint32_t q;
    if (!parse_uint(f[6], 9, &q)) { return NMEA_ERR_MALFORMED; }
    out->fix_quality = (uint8_t)q;

    uint32_t sats = 0;
    if (f[7][0] != '\0' && !parse_uint(f[7], 99, &sats)) { sats = 99; }
    out->sats = (uint8_t)sats;

    out->time_valid = parse_time(f[1], &out->utc_hhmmss);

    if (out->fix_quality > 0) {
        int32_t lat, lon;
        if (parse_coord(f[2], f[3], 4, 90, 'N', 'S', &lat) &&
            parse_coord(f[4], f[5], 5, 180, 'E', 'W', &lon)) {
            out->lat_e6 = lat;
            out->lon_e6 = lon;
            out->position_valid = true;
        }
        out->alt_valid = parse_tenths(f[9], &out->alt_dm);
    }
    return NMEA_OK;
}
