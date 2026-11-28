#include "imu_processor.h"

#include "telemetry_data.h"
#include "app/fsm.h"
#include "processing/attitude.h"
#include "processing/motion.h"
#include "processing/statistics.h"
#include "processing/tracker_status.h"

static int64_t last_sample_ms;

void imu_processor_init(void)
{
    last_sample_ms = 0;
    attitude_reset();
    motion_reset();
}

void imu_processor_process(const imu_data_t *imu,
                           const imu_data_t *corrected_imu,
                           int64_t now_ms)
{
    telemetry_t status_telemetry;
    attitude_t attitude;
    float dt_s;

    if (fsm_get_mode() == FSM_MODE_NORMAL) {
        statistics_add_imu(
            imu->accel_x,
            imu->accel_y,
            imu->accel_z,
            corrected_imu->gyro_x,
            corrected_imu->gyro_y,
            corrected_imu->gyro_z
        );
    }

    if (last_sample_ms == 0) {
        dt_s = 0.020f;
    } else {
        dt_s = (float)(now_ms - last_sample_ms) / 1000.0f;
    }

    last_sample_ms = now_ms;

    telemetry_set_imu(
        (int32_t)(imu->accel_x * 100.0f),
        (int32_t)(imu->accel_y * 100.0f),
        (int32_t)(imu->accel_z * 100.0f),
        (int32_t)(corrected_imu->gyro_x * 10.0f),
        (int32_t)(corrected_imu->gyro_y * 10.0f),
        (int32_t)(corrected_imu->gyro_z * 10.0f)
    );

    if (attitude_update(
            imu->accel_x,
            imu->accel_y,
            imu->accel_z,
            corrected_imu->gyro_x,
            corrected_imu->gyro_y,
            corrected_imu->gyro_z,
            dt_s,
            &attitude) == 0) {

        telemetry_set_attitude(
            (int32_t)(attitude.roll_deg * 100.0f),
            (int32_t)(attitude.pitch_deg * 100.0f),
            (int32_t)(attitude.yaw_deg * 100.0f)
        );

        motion_t motion;
        if (motion_update(
                imu->accel_x,
                imu->accel_y,
                imu->accel_z,
                corrected_imu->gyro_x,
                corrected_imu->gyro_y,
                corrected_imu->gyro_z,
                attitude.roll_deg,
                attitude.pitch_deg,
                attitude.yaw_deg,
                dt_s,
                &motion) == 0) {
            telemetry_set_local(
                (int32_t)(motion.x_m * 100.0f),
                (int32_t)(motion.y_m * 100.0f),
                (int32_t)(motion.z_m * 100.0f),
                (int32_t)(motion.vx_ms * 100.0f),
                (int32_t)(motion.vy_ms * 100.0f),
                (int32_t)(motion.vz_ms * 100.0f)
            );
        }
    }

    telemetry_flag_clear(FLAG_IMU_INVALID);

    telemetry_get(&status_telemetry);

    tracker_status_update(
        imu->accel_x,
        imu->accel_y,
        imu->accel_z,
        corrected_imu->gyro_x,
        corrected_imu->gyro_y,
        corrected_imu->gyro_z,
        status_telemetry.flags
    );
}
