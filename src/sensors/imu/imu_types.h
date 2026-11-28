#ifndef IMU_TYPES_H
#define IMU_TYPES_H

#include <stdint.h>

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} imu_raw_data_t;

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;

    float gyro_x;
    float gyro_y;
    float gyro_z;
} imu_data_t;

#endif /* IMU_TYPES_H */
