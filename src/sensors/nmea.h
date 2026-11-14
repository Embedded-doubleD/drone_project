#ifndef NMEA_H
#define NMEA_H

#include <stdbool.h>
#include <stdint.h>

/* Codigos de retorno de nmea_parse_gga() */
#define NMEA_OK            0
#define NMEA_ERR_ARG      -1   /* puntero nulo */
#define NMEA_ERR_FORMAT   -2   /* sin '$' o '*', checksum erroneo o linea demasiado larga */
#define NMEA_ERR_NOT_GGA  -3   /* frase valida, pero no es GGA */
#define NMEA_ERR_MALFORMED -4  /* GGA con campos ausentes o ilegibles */

typedef struct {
    bool     time_valid;
    uint32_t utc_hhmmss;     /* p. ej. 133641 */
    uint8_t  fix_quality;    /* 0 = sin fix */
    uint8_t  sats;           /* satelites usados, saturado a 99 */
    bool     position_valid; /* true solo si fix_quality > 0 y lat/lon legibles */
    int32_t  lat_e6;         /* grados * 1e6, negativo = Sur */
    int32_t  lon_e6;         /* grados * 1e6, negativo = Oeste */
    bool     alt_valid;
    int32_t  alt_dm;         /* metros * 10 */
} nmea_gga_t;

/* Comprueba "$....*HH". La linea llega sin \r\n. */
bool nmea_checksum_ok(const char *line);

/* Interpreta una frase GGA (con cualquier talker: GP, GN...). */
int nmea_parse_gga(const char *line, nmea_gga_t *out);

#endif /* NMEA_H */
