#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "imu_calibration.h"
#include "imu_driver.h"
#include "imu_interrupt.h"
#include "imu_processor.h"
#include "imu_recovery.h"
#include "imu_service.h"
#include "telemetry_data.h"
#include "processing/motion.h"
#include "processing/tracker_status.h"

#define IMU_STACK_SIZE              1024
#define IMU_PRIORITY                6
#define IMU_DATA_READY_TIMEOUT_MS   500
/*
 * IMU acquisition thread.
 * Sleeps until the MPU6050 signals Data Ready.
 */
static void imu_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    imu_data_t imu;
    imu_data_t corrected_imu;
    imu_calibration_t calibration;
    int ret;

    imu_processor_init();

    /* Initialize GPIO first, while sensor interrupts are disabled. */
    ret = imu_interrupt_init();

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    /* Initialize MPU6050 */
    while ((ret = imu_init()) < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        k_msleep(1000);
    }

    /* Enable the MCU GPIO interrupt first. */
    ret = imu_interrupt_enable();

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    /* Then enable Data Ready interrupt on the MPU6050. */
    ret = imu_enable_data_ready_interrupt();

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    /*
     * Calibrate the gyroscope before publishing measurements.
     * If calibration fails, retry while keeping the IMU invalid.
     */
    while (1) {
        ret = imu_calibration_run(&calibration);

        if (ret == 0) {
            break;
        }

        telemetry_flag_set(FLAG_IMU_INVALID);
        k_msleep(1000);
    }

    telemetry_flag_set(FLAG_YAW_UNRELIABLE);
    telemetry_flag_set(FLAG_INERTIAL_ONLY);
    (void)motion_set_gravity(calibration.accel_norm_ref);

    tracker_status_init();
    (void)tracker_status_set_gravity(calibration.accel_norm_ref);

    /* Normal acquisition loop */
    while (1) {
        ret = imu_interrupt_wait(K_MSEC(IMU_DATA_READY_TIMEOUT_MS));

        if (ret != 0) {
            /*
             * Report the failure before attempting recovery.  Otherwise
             * the old FLYING state can remain visible while the recovery
             * loop is retrying the disconnected sensor.
             */
            tracker_status_imu_error();
            (void)imu_recover();
            continue;
        }

        ret = imu_read(&imu);

        if (ret == 0) {
            imu_calibration_correct_gyro(
                &calibration,
                &imu,
                &corrected_imu
            );

            imu_processor_process(
                &imu,
                &corrected_imu,
                k_uptime_get()
            );
        } else {
            tracker_status_imu_error();
        }
    }
}

K_THREAD_DEFINE(
    imu_thread_id,
    IMU_STACK_SIZE,
    imu_thread,
    NULL, NULL, NULL,
    IMU_PRIORITY,
    0,
    0
);
