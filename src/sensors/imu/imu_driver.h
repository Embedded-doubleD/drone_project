#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include "imu_types.h"

int imu_init(void);
int imu_read_raw(imu_raw_data_t *data);
int imu_read(imu_data_t *data);
int imu_enable_data_ready_interrupt(void);

#endif /* IMU_DRIVER_H */
