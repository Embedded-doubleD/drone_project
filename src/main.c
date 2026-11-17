
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

    ret = button_service_init();

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

    int64_t next_light = k_uptime_get();
    int64_t next_env = k_uptime_get();

    while (1) {
        int64_t now = k_uptime_get();

        fsm_process();

        if (ldr_ready && now >= next_light) {
            ldr_service_read();
            next_light = now + LIGHT_PERIOD_MS;
        }

        if (env_ready && now >= next_env) {
            int32_t temp_dc;
            int32_t hum_dpct;

            temp_hum_read(&temp_dc, &hum_dpct);
            next_env = now + ENV_PERIOD_MS;
        }

        k_sleep(K_MSEC(10));
    }

    return 0;
}
