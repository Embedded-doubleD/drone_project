#ifndef IMU_H
#define IMU_H

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

int imu_init(void);
int imu_read_raw(imu_raw_data_t *data);
int imu_read(imu_data_t *data);

#endif