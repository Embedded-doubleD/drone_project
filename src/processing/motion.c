#include "motion.h"

#include <errno.h>
#include <math.h>
#include <stddef.h>

#define MOTION_G_M_S2          9.80665f
#define MOTION_DEG_TO_RAD      (3.14159265358979323846f / 180.0f)
#define MOTION_MAX_DT_S        0.10f

/* PROVISIONALES: hay que medirlos con la placa quieta. */
#define MOTION_STILL_ACCEL_TOL 0.5f   /* |aceleracion lineal| maxima, m/s2 */
#define MOTION_STILL_GYRO_DPS  3.0f   /* |g| maximo en reposo, deg/s */
#define MOTION_STILL_SAMPLES   25     /* 0,5 s a 50 Hz              */

static motion_t state;
static int still_count;
static float g_ref = MOTION_G_M_S2;

int motion_set_gravity(float g_m_s2)
{
    if (!isfinite(g_m_s2) ||
        (g_m_s2 < 0.9f * MOTION_G_M_S2) || (g_m_s2 > 1.1f * MOTION_G_M_S2)) {
        return -EINVAL;
    }
    g_ref = g_m_s2;
    return 0;
}

void motion_reset(void)
{
    state = (motion_t){0};
    still_count = 0;
}

int motion_update(float ax, float ay, float az,
                  float gx_dps, float gy_dps, float gz_dps,
                  float roll_deg, float pitch_deg, float yaw_deg,
                  float dt_s, motion_t *out)
{
    if (out == NULL) {
        return -EINVAL;
    }
    if (!isfinite(ax) || !isfinite(ay) || !isfinite(az) ||
        !isfinite(gx_dps) || !isfinite(gy_dps) || !isfinite(gz_dps) ||
        !isfinite(roll_deg) || !isfinite(pitch_deg) || !isfinite(yaw_deg) ||
        !isfinite(dt_s) || (dt_s <= 0.0f)) {
        return -EINVAL;
    }

    /* Hueco largo entre muestras: no se integra y la velocidad se descarta. */
    if (dt_s > MOTION_MAX_DT_S) {
        state.vx_ms = state.vy_ms = state.vz_ms = 0.0f;
        still_count = 0;
        state.stationary = false;
        *out = state;
        return 0;
    }

    const float phi = roll_deg * MOTION_DEG_TO_RAD;
    const float theta = pitch_deg * MOTION_DEG_TO_RAD;
    const float psi = yaw_deg * MOTION_DEG_TO_RAD;
    const float sp = sinf(phi),   cp = cosf(phi);
    const float st = sinf(theta), ct = cosf(theta);
    const float sy = sinf(psi),   cy = cosf(psi);

    /* Sensor -> local (ZYX), eje Z hacia arriba. */
    const float awx = (ct * cy) * ax + (sp * st * cy - cp * sy) * ay + (cp * st * cy + sp * sy) * az;
    const float awy = (ct * sy) * ax + (sp * st * sy + cp * cy) * ay + (cp * st * sy - sp * cy) * az;
    const float awz = (-st) * ax + (sp * ct) * ay + (cp * ct) * az - g_ref;

    const float lin_norm = sqrtf(awx * awx + awy * awy + awz * awz);
    const float g_norm = sqrtf(gx_dps * gx_dps + gy_dps * gy_dps + gz_dps * gz_dps);
    const bool still = (lin_norm < MOTION_STILL_ACCEL_TOL) &&
                       (g_norm < MOTION_STILL_GYRO_DPS);

    still_count = still ? (still_count + 1) : 0;
    state.stationary = (still_count >= MOTION_STILL_SAMPLES);

    if (state.stationary) {
        state.vx_ms = state.vy_ms = state.vz_ms = 0.0f;
    } else {    /* Integración de Euler para velocidad y posición */
        state.vx_ms += awx * dt_s;
        state.vy_ms += awy * dt_s;
        state.vz_ms += awz * dt_s;
        state.x_m += state.vx_ms * dt_s;
        state.y_m += state.vy_ms * dt_s;
        state.z_m += state.vz_ms * dt_s;
    }

    *out = state;
    return 0;
}