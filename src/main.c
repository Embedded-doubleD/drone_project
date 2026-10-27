#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

/* Power policy API: used only when CONFIG_PM is enabled. */
#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

/*
 * Identifier of the STOP2 substate defined for STM32WL. This state is
 * prevented because, according to the tests described in the README, the ADC
 * cannot complete conversions correctly after resuming from STOP2.
 */
#define PM_SUBSTATE_STOP2 3

int main(void)
{
  
  #ifdef CONFIG_PM
    /*
     * Prevents entry into STOP2 for the remainder of execution. Other
     * low-power states that are compatible with the ADC remain allowed.
     */
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_SUBSTATE_STOP2);
  #endif

  return 0;
}
