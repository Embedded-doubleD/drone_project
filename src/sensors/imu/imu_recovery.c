#include "imu_recovery.h"

#include "imu_driver.h"
#include "imu_interrupt.h"
#include "processing/tracker_status.h"

#include <zephyr/kernel.h>

#define IMU_RECOVERY_DELAY_MS 100

int imu_recover(void)
{
    int ret;

    while (1) {
        imu_interrupt_reset();

        ret = imu_init();

        if (ret == 0) {
            /*
             * Re-apply the MCU-side interrupt configuration after
             * reconnecting the IMU.  This is important when
             * the sensor was power-cycled or SDA was disconnected.
             */
            ret = imu_interrupt_enable();
        }

        if (ret == 0) {
            ret = imu_enable_data_ready_interrupt();
        }

        if (ret == 0) {
            return 0;
        }

        tracker_status_imu_error();
        k_msleep(IMU_RECOVERY_DELAY_MS);
    }
}
