#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <stdint.h>

typedef enum {
    TRACKER_IDLE = 0,
    TRACKER_FLYING,
    TRACKER_PROBLEM,
} tracker_state_t;

/* Bits del campo FLAGS, tal como los define la tabla de flags del enunciado */
#define FLAG_IMU_INVALID    0x0001u
#define FLAG_GPS_INVALID    0x0002u
#define FLAG_LDR_INVALID    0x0004u
#define FLAG_ENV_INVALID    0x0008u
#define FLAG_IMPACT         0x0010u
#define FLAG_CAL_PENDING    0x0020u
#define FLAG_INERTIAL_ONLY  0x0040u
#define FLAG_YAW_UNRELIABLE 0x0080u

typedef struct {
    /* GPS */
    uint8_t  gps_fix;     /* 0 = sin fix, 1 = fix valido */
    uint8_t  gps_sats;
    int32_t  lat_e6;      /* grados * 1e6 */
    int32_t  lon_e6;
    int32_t  alt_dm;      /* metros * 10 */
    /* Posicion y velocidad locales */
    int32_t  x_cm, y_cm, z_cm;
    int32_t  vx_cms, vy_cms, vz_cms;
    /* Actitud */
    int32_t  roll_cdeg, pitch_cdeg, yaw_cdeg;
    /* IMU */
    int32_t  ax_cms2, ay_cms2, az_cms2;   /* m/s2 * 100 */
    int32_t  gx_ddegs, gy_ddegs, gz_ddegs; /* deg/s * 10 */
    /* Ambiente */
    int32_t  light_dpct;  /* % * 10 */
    int32_t  temp_dc;     /* grados C * 10 */
    int32_t  hum_dpct;    /* % * 10 */
    tracker_state_t state;
    uint16_t flags;
} telemetry_t;

void telemetry_get(telemetry_t *out);

void telemetry_set_gps(uint8_t fix, uint8_t sats,
                       int32_t lat_e6, int32_t lon_e6, int32_t alt_dm);
void telemetry_set_light(int32_t light_dpct);
void telemetry_set_env(int32_t temp_dc, int32_t hum_dpct);
void telemetry_set_state(tracker_state_t state);

void telemetry_flag_set(uint16_t mask);
void telemetry_flag_clear(uint16_t mask);

#endif /* TELEMETRY_DATA_H */