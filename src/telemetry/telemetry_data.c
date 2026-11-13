#include <zephyr/kernel.h>
#include <string.h>
#include "telemetry_data.h"

/* Se inicializan en compilacion: no hace falta una funcion init */
K_MUTEX_DEFINE(telemetry_lock);

static telemetry_t data = {
    .flags = FLAG_IMU_INVALID | FLAG_GPS_INVALID | FLAG_LDR_INVALID |
             FLAG_ENV_INVALID | FLAG_CAL_PENDING,
    .state = TRACKER_IDLE,
};

void telemetry_get(telemetry_t *out)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    memcpy(out, &data, sizeof(*out));
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_set_gps(uint8_t fix, uint8_t sats,
                       int32_t lat_e6, int32_t lon_e6, int32_t alt_dm)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.gps_fix = fix;
    data.gps_sats = sats;
    if (fix) {                      /* sin fix se conservan los ultimos validos */
        data.lat_e6 = lat_e6;
        data.lon_e6 = lon_e6;
        data.alt_dm = alt_dm;
        data.flags &= ~FLAG_GPS_INVALID;
    } else {
        data.flags |= FLAG_GPS_INVALID;
    }
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_set_light(int32_t light_dpct)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.light_dpct = light_dpct;
    data.flags &= ~FLAG_LDR_INVALID;
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_set_env(int32_t temp_dc, int32_t hum_dpct)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.temp_dc = temp_dc;
    data.hum_dpct = hum_dpct;
    data.flags &= ~FLAG_ENV_INVALID;
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_set_state(tracker_state_t state)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.state = state;
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_flag_set(uint16_t mask)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.flags |= mask;
    k_mutex_unlock(&telemetry_lock);
}

void telemetry_flag_clear(uint16_t mask)
{
    k_mutex_lock(&telemetry_lock, K_FOREVER);
    data.flags &= ~mask;
    k_mutex_unlock(&telemetry_lock);
}