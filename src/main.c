#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <stdbool.h>
#include <stdint.h>

#include "app/events.h"
#include "input/button_service.h"
#include "sensors/ldr_service.h"
#include "sensors/temp_hum_service.h"
#include "fsm.h"

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

#define PM_SUBSTATE_STOP2       3
#define SENSOR_PERIOD_MS        3000
#define MAIN_LOOP_SLEEP_MS      10

int main(void)
{
    int ret;
    bool ldr_ready = false;
    bool env_ready = false;

    events_init();

#ifdef CONFIG_PM
    /*
     * Prevents entry into STOP2.
     * Other compatible low-power states remain allowed.
     */
    pm_policy_state_lock_get(
        PM_STATE_SUSPEND_TO_IDLE,
        PM_SUBSTATE_STOP2
    );
#endif

    /* Initialize button service. */
    ret = button_service_init();

    if (ret != 0) {
        printk("Button initialization failed: %d\n", ret);
    }

    /* Initialize LDR service. */
    ret = ldr_service_init();

    if (ret == 0) {
        ldr_ready = true;
    } else {
        printk("LDR initialization failed: %d\n", ret);
    }

    /* Initialize Si7021 service. */
    ret = temp_hum_init();

    if (ret == 0) {
        env_ready = true;
    } else {
        printk("Si7021 initialization failed: %d\n", ret);
    }

    /* Initialize FSM in the main thread. */
    ret = fsm_init();

    if (ret != 0) {
        printk("FSM initialization failed: %d\n", ret);
    }

    int64_t next_sensor_read = 0;

    while (1) {

        /* Process button events and FSM logic. */
        fsm_process();

        /* Periodic sensor acquisition. */
        int64_t now = k_uptime_get();

        if (now >= next_sensor_read) {

            if (ldr_ready) {
                ret = ldr_service_read();

                if (ret != 0) {
                    printk("LDR acquisition failed: %d\n", ret);
                }
            }

            if (env_ready) {
                int32_t temp_dc;
                int32_t hum_dpct;

                ret = temp_hum_read(&temp_dc, &hum_dpct);

                if (ret != 0) {
                    printk("Si7021 acquisition failed: %d\n", ret);
                }
            }

            next_sensor_read = k_uptime_get() + SENSOR_PERIOD_MS;
        }

        k_msleep(MAIN_LOOP_SLEEP_MS);
    }

    return 0;
}