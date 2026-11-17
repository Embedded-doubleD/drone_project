#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "app/events.h"
#include "input/button_service.h"
#include "sensors/ldr_service.h"

/* Power policy API: used only when CONFIG_PM is enabled. */
#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

#define PM_SUBSTATE_STOP2 3

int main(void)
{
    int ret;

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

    if (button_service_init() != 0) {
        printk("Button init failed\n");
    }

    ret = ldr_service_init();

    if (ret != 0) {
        printk("LDR initialization failed: %d\n", ret);
        return 0;
    }

    /* One single LDR acquisition. */
    ret = ldr_service_read();

    if (ret != 0) {
        printk("LDR acquisition failed: %d\n", ret);
    }

    return 0;
}