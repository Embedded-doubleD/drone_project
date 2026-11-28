#ifndef IMU_CALIBRATION_H
#define IMU_CALIBRATION_H

#include "imu_types.h"

typedef struct {
    float gyro_offset_x;
    float gyro_offset_y;
    float gyro_offset_z;
    float accel_norm_ref;
} imu_calibration_t;

int imu_calibration_run(imu_calibration_t *calibration);
void imu_calibration_correct_gyro(const imu_calibration_t *calibration,
                                  const imu_data_t *input,
                                  imu_data_t *output);

#endif /* IMU_CALIBRATION_H */
