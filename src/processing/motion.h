#ifndef MOTION_H
#define MOTION_H

#include <stdbool.h>

typedef struct {
    float x_m, y_m, z_m; // Position in meters
    float vx_ms, vy_ms, vz_ms; // Velocity in meters per second
    bool stationary;
} motion_t;

/* Pone posición y velocidad a cero. */
void motion_reset(void);

/*
    Fija la gravedad de referencia (m/s2) medida con la placa quieta al arrancar.
    Devuelve -EINVAL si queda fuera de +-10% de 9,80665 (placa en movimiento). 
*/
int motion_set_gravity(float g_m_s2);

/*
    Integra la aceleración (m/s2, ejes del sensor) con la orientación (grados)
    compensando la gravedad. Devuelve 0, o -EINVAL si algun dato no es valido.
*/
int motion_update(float ax, float ay, float az,
                float gx_dps, float gy_dps, float gz_dps,
                float roll_deg, float pitch_deg, float yaw_deg,
                float dt_s, motion_t *out);

#endif /* MOTION_H */