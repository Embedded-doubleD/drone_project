#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <stdint.h>

typedef struct {
    uint32_t gga_ok;        /* GGA validas procesadas */
    uint32_t lines_bad;     /* checksum erroneo o GGA ilegible */
    uint32_t lines_dropped; /* lineas perdidas por cola llena */
} gps_stats_t;

/* El servicio arranca solo (K_THREAD_DEFINE), igual que ldr_service y fsm.
Publica en telemetry_data con telemetry_set_gps(). */
void gps_service_get_stats(gps_stats_t *out);

#endif /* GPS_SERVICE_H */