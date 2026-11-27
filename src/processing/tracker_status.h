#ifndef TRACKER_STATUS_H
#define TRACKER_STATUS_H

#include <stdint.h>

#include "telemetry_data.h"

void tracker_status_init(void);

void tracker_status_update(float ax, float ay, float az,
                           float gx, float gy, float gz,
                           uint16_t flags);

void tracker_status_imu_error(void);
void tracker_status_gps_communication_error(void);
void tracker_status_gps_communication_recovered(void);

#endif /* TRACKER_STATUS_H */
