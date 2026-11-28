#include "imu_calibration.h"

#include "imu_driver.h"
#include "imu_interrupt.h"

#include <zephyr/kernel.h>

#include <errno.h>
#include <math.h>

/* Gyroscope calibration */
#define IMU_CALIBRATION_SAMPLES       200
#define IMU_CALIBRATION_TIMEOUT_MS    500
#define IMU_CALIBRATION_MAX_ERRORS    10

/*
 * Calibrate the gyroscope at startup.
 *
 * The board must remain completely still during calibration.
 * Collects 200 valid samples and calculates the average
 * gyroscope value for each axis.
 *
 * Offsets are stored in degrees/second.
 */
int imu_calibration_run(imu_calibration_t *calibration)
{
    imu_data_t imu;

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    float sum_norm = 0.0f;

    int samples = 0;
    int errors = 0;
    int ret;

    if (calibration == NULL) {
        return -EINVAL;
    }

    while (samples < IMU_CALIBRATION_SAMPLES) {

        ret = imu_interrupt_wait(K_MSEC(IMU_CALIBRATION_TIMEOUT_MS));

        if (ret != 0) {
            return -ETIMEDOUT;
        }

        ret = imu_read(&imu);

        if (ret < 0) {
            errors++;

            if (errors >= IMU_CALIBRATION_MAX_ERRORS) {
                return ret;
            }

            continue;
        }

        sum_norm += sqrtf(imu.accel_x*imu.accel_x + imu.accel_y*imu.accel_y + imu.accel_z*imu.accel_z);

        sum_x += imu.gyro_x;
        sum_y += imu.gyro_y;
        sum_z += imu.gyro_z;

        samples++;
    }

    calibration->gyro_offset_x = sum_x / (float)IMU_CALIBRATION_SAMPLES;
    calibration->gyro_offset_y = sum_y / (float)IMU_CALIBRATION_SAMPLES;
    calibration->gyro_offset_z = sum_z / (float)IMU_CALIBRATION_SAMPLES;

    calibration->accel_norm_ref = sum_norm / (float)IMU_CALIBRATION_SAMPLES;

    return 0;
}

void imu_calibration_correct_gyro(const imu_calibration_t *calibration,
                                  const imu_data_t *input,
                                  imu_data_t *output)
{
    *output = *input;
    output->gyro_x = input->gyro_x - calibration->gyro_offset_x;
    output->gyro_y = input->gyro_y - calibration->gyro_offset_y;
    output->gyro_z = input->gyro_z - calibration->gyro_offset_z;
}
