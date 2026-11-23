
#include <zephyr/kernel.h>

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

#include "app/events.h"
#include "app/fsm.h"
#include "input/button_service.h"
#include "sensors/ldr_service.h"
#include "sensors/temp_hum_service.h"
#include "telemetry_data.h"

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

#define PM_SUBSTATE_STOP2 3

int main(void)
{
    int ret;
    bool ldr_ready = false;
    bool env_ready = false;

    events_init();

#ifdef CONFIG_PM
    pm_policy_state_lock_get(
        PM_STATE_SUSPEND_TO_IDLE,
        PM_SUBSTATE_STOP2
    );
#endif

    button_service_init();

    ret = ldr_service_init();
    if (ret != 0) {
        telemetry_flag_set(FLAG_LDR_INVALID);
    } else {
        ldr_ready = true;
    }

    ret = temp_hum_init();
    if (ret != 0) {
        telemetry_flag_set(FLAG_ENV_INVALID);
    } else {
        env_ready = true;
    }

    ret = fsm_init();
    if (ret != 0) {
        return 0;
    }

    int64_t now = k_uptime_get();

    int64_t next_light = now;
    int64_t next_env = now;
    int64_t next_led = now + LED_REFRESH_MS;

    while (1) {
        now = k_uptime_get();

        /* LDR acquisition */
        if (ldr_ready && now >= next_light) {
            ret = ldr_service_read();

            now = k_uptime_get();
            do {
                next_light += LIGHT_PERIOD_MS;
            } while (next_light <= now);
        }

        /* Si7021 acquisition */
        if (env_ready && now >= next_env) {
            int32_t temp_dc;
            int32_t hum_dpct;

            ret = temp_hum_read(&temp_dc, &hum_dpct);

            now = k_uptime_get();
            do {
                next_env += ENV_PERIOD_MS;
            } while (next_env <= now);
        }

        /* Calculate the nearest deadline */
        now = k_uptime_get();

        int64_t wake = next_led;

        if (ldr_ready && next_light < wake) {
            wake = next_light;
        }

        if (env_ready && next_env < wake) {
            wake = next_env;
        }

        int64_t wait = wake - now;

        if (wait < 0) {
            wait = 0;
        }

        /* Wait for an event or the next scheduled acquisition */
        fsm_process(K_MSEC(wait));

        /* Advance LED deadline */
        now = k_uptime_get();

        if (now >= next_led) {
            do {
                next_led += LED_REFRESH_MS;
            } while (next_led <= now);
        }
    }

    return 0;
}
