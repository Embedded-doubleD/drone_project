#ifndef TRACKER_STATUS_H
#define TRACKER_STATUS_H

#include <stdint.h>

#include "telemetry_data.h"

void tracker_status_init(void);

int tracker_status_set_gravity(float g_m_s2);

void tracker_status_update(float ax, float ay, float az,
                           float gx, float gy, float gz,
                           uint16_t flags);

void tracker_status_imu_error(void);

#endif /* TRACKER_STATUS_H */
