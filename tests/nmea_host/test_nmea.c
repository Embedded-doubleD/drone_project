#include <stdio.h>
#include <string.h>
#include "nmea.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void make_line(char *out, const char *body)
{
    unsigned cs = 0;
    for (const char *c = body; *c; c++) { cs ^= (unsigned char)*c; }
    sprintf(out, "$%s*%02X", body, cs);
}

int main(void)
{
    nmea_gga_t g;
    char line[128];

    /* Capturas reales del modulo (usart1, 9600 baud) */
    CHECK(nmea_parse_gga("$GPGGA,130328.086,,,,,0,00,,,M,,M,,*7D", &g) == NMEA_OK);
    CHECK(g.fix_quality == 0 && g.sats == 0 && !g.position_valid && !g.alt_valid);
    CHECK(g.time_valid && g.utc_hhmmss == 130328);

    CHECK(nmea_parse_gga("$GPGGA,133641.000,4019.8775,N,00330.5797,W,1,05,2.25,271.5,M,51.7,M,,*73", &g) == NMEA_OK);
    CHECK(g.fix_quality == 1 && g.sats == 5 && g.position_valid);
    CHECK(g.lat_e6 == 40331292);
    CHECK(g.lon_e6 == -3509662);
    CHECK(g.alt_valid && g.alt_dm == 2715);
    CHECK(g.utc_hhmmss == 133641);

    CHECK(nmea_parse_gga("$GPGGA,133642.000,4019.8776,N,00330.5797,W,1,05,2.25,271.6,M,51.7,M,,*70", &g) == NMEA_OK);
    CHECK(g.lat_e6 == 40331293 && g.alt_dm == 2716);

    /* Rechazos */
    CHECK(nmea_parse_gga("$GPGGA,133642.000,4019.8776,N,00330.5797,W,1,05,2.25,271.6,M,51.7,M,,*71", &g) == NMEA_ERR_FORMAT);
    CHECK(nmea_parse_gga("$GPGGA,133642.000,4019.8776,N", &g) == NMEA_ERR_FORMAT);
    CHECK(nmea_parse_gga("", &g) == NMEA_ERR_FORMAT);
    CHECK(nmea_parse_gga("$GPRMC,133641.000,A,4019.8775,N,00330.5797,W,0.52,94.54,131126,,,A*4E", &g) == NMEA_ERR_NOT_GGA);
    CHECK(nmea_parse_gga("$GPGSA,A,1,,,,,,,,,,,,,,,*1E", &g) == NMEA_ERR_NOT_GGA);
    CHECK(nmea_parse_gga(NULL, &g) == NMEA_ERR_ARG);

    /* Otros talkers, hemisferios y casos limite (checksum calculado por el test) */
    make_line(line, "GNGGA,000000.000,3352.1234,S,15112.5678,E,1,08,1.0,10.0,M,0.0,M,,");
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && g.position_valid);
    CHECK(g.lat_e6 == -33868723 && g.lon_e6 == 151209463);   /* 33+52.1234/60 ; 151+12.5678/60 */
    CHECK(g.sats == 8 && g.alt_dm == 100);

    make_line(line, "GPGGA,120000.00,4019.8775,N,00330.5797,W,1,05,2.25,-12.3,M,51.7,M,,");
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && g.alt_dm == -123);

    make_line(line, "GPGGA,120000.00,4019.8775,N,00330.5797,W,0,00,,,M,,M,,");       /* calidad 0 con coordenadas */
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && !g.position_valid && g.fix_quality == 0);

    make_line(line, "GPGGA,120000.00,4099.0000,N,00330.5797,W,1,05,2.25,1.0,M,0,M,,");  /* minutos >= 60 */
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && !g.position_valid);

    make_line(line, "GPGGA,120000.00,4019.8775,X,00330.5797,W,1,05,2.25,1.0,M,0,M,,");  /* hemisferio invalido */
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && !g.position_valid);

    make_line(line, "GPGGA,120000.00,,,,,1,05,2.25,,M,,M,,");                          /* fix sin coordenadas */
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && !g.position_valid);

    make_line(line, "GPGGA,120000.00,9000.0000,N,18000.0000,E,1,05,2.25,1.0,M,0,M,,"); /* limites exactos */
    CHECK(nmea_parse_gga(line, &g) == NMEA_OK && g.position_valid && g.lat_e6 == 90000000 && g.lon_e6 == 180000000);

    make_line(line, "GPGGA,120000.00,4019.8775,N,00330.5797,W,1");                      /* pocos campos */
    CHECK(nmea_parse_gga(line, &g) == NMEA_ERR_MALFORMED);

    printf("%s (%d fallos)\n", failures ? "FALLO" : "OK", failures);
    return failures != 0;
}
