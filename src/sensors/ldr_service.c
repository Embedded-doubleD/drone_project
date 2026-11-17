
#include <errno.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "ldr_service.h"
#include "events.h"
#include "telemetry_data.h"
#include "config.h"

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>

/*
 * Prevent the MCU from entering STOP/SUSPEND
 * while the LDR acquisition is in progress.
 */
#define LDR_PM_LOCK() \
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)

#define LDR_PM_UNLOCK() \
    pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)

#else
#define LDR_PM_LOCK()   do { } while (0)
#define LDR_PM_UNLOCK() do { } while (0)
#endif

/* ADC channel defined through Devicetree. */
#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc =
    ADC_DT_SPEC_GET(ADC_NODE);

/* GPIO powering the LDR voltage divider. */
#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr =
    GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

/* Sensor acquisition configuration. */
#define LDR_SETTLE_MS 1

static int16_t sample;

static struct adc_sequence sequence = {
    .buffer = &sample,
    .buffer_size = sizeof(sample),
};

static bool initialized = false;


/* Converts calibrated raw values to tenths of a percent (0..1000). */
static int32_t ldr_raw_to_dpct(uint16_t raw)
{
    int32_t span =
        (int32_t)LDR_RAW_BRIGHT - (int32_t)LDR_RAW_DARK;

    if (span <= 0) {
        return 0;
    }

    int32_t value =
        (((int32_t)raw - (int32_t)LDR_RAW_DARK) * 1000) / span;

    if (value < 0) {
        value = 0;
    } else if (value > 1000) {
        value = 1000;
    }

    return value;
}


/* Initializes the ADC and GPIO peripherals. */
int ldr_service_init(void)
{
    if (initialized) {
        return 0;
    }

    if (!adc_is_ready_dt(&ldr_adc)) {
        printk("Error: LDR ADC device is not ready\n");
        return -ENODEV;
    }

    int ret = adc_channel_setup_dt(&ldr_adc);

    if (ret != 0) {
        printk("Error: LDR ADC channel setup failed: %d\n", ret);
        return ret;
    }

    ret = adc_sequence_init_dt(&ldr_adc, &sequence);

    if (ret != 0) {
        printk("Error: LDR ADC sequence setup failed: %d\n", ret);
        return ret;
    }

    if (!gpio_is_ready_dt(&ldr_pwr)) {
        printk("Error: LDR power GPIO is not ready\n");
        return -ENODEV;
    }

    /* Keep the divider unpowered between acquisitions. */
    ret = gpio_pin_configure_dt(
        &ldr_pwr,
        GPIO_OUTPUT_INACTIVE
    );

    if (ret != 0) {
        printk("Error: LDR power GPIO configuration failed: %d\n", ret);
        return ret;
    }

    initialized = true;

    return 0;
}


/* Performs one synchronous acquisition and publishes its event. */
int ldr_service_read(void)
{
    printk("DEBUG: ldr_service_read called\n");
    
    if (!initialized) {
        return -EACCES;
    }

    event_t event = {0};
    int ret;

    /*
     * Prevent low-power suspend during voltage-divider
     * stabilization and ADC conversion.
     */
    LDR_PM_LOCK();

    /* Power on the voltage divider. */
    ret = gpio_pin_set_dt(&ldr_pwr, 1);

    if (ret == 0) {
        k_msleep(LDR_SETTLE_MS);

        /* Perform ADC conversion. */
        ret = adc_read_dt(&ldr_adc, &sequence);
        printk("DEBUG: LDR raw = %d\n", sample);
    }

    /*
     * Always request power-off, even if acquisition fails.
     */
    int power_off_ret = gpio_pin_set_dt(&ldr_pwr, 0);

    LDR_PM_UNLOCK();

    if (ret == 0 && power_off_ret != 0) {
        ret = power_off_ret;
    }

    if (ret != 0) {
        telemetry_flag_set(FLAG_LDR_INVALID);
        return ret;
    }

    /* Protect against negative ADC values. */
    int32_t value = sample < 0 ? 0 : sample;

    event.type = EVENT_ADC_READING;
    event.adc_raw = (uint16_t)value;

    event.brightness_pct =
        (uint16_t)MIN(
            (value * 100) / ADC_FULL_SCALE,
            100
        );

    /* Update telemetry brightness using calibrated values. */
    telemetry_set_light(
        ldr_raw_to_dpct(event.adc_raw)
    );

    /* Check whether the reading is outside the calibrated range. */
    if (event.adc_raw < LDR_RAW_DARK ||
        event.adc_raw > LDR_RAW_BRIGHT) {

        telemetry_flag_set(FLAG_LDR_INVALID);
    }

#if LDR_CALIBRATED
    telemetry_flag_clear(FLAG_CAL_PENDING);
#endif

    /* Publish the acquisition result to the application event queue. */
    return events_send(&event);
}
