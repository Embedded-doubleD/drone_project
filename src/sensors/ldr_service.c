
#include <errno.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include "ldr_service.h"
#include "events.h"
#include "telemetry_data.h"
#include "config.h"

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>

#define LDR_PM_LOCK() \
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)

#define LDR_PM_UNLOCK() \
    pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)

#else
#define LDR_PM_LOCK()   do { } while (0)
#define LDR_PM_UNLOCK() do { } while (0)
#endif

#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc =
    ADC_DT_SPEC_GET(ADC_NODE);

#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr =
    GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

#define LDR_SETTLE_MS 1

static int16_t sample;

static struct adc_sequence sequence = {
    .buffer = &sample,
    .buffer_size = sizeof(sample),
};

static bool initialized = false;

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

int ldr_service_init(void)
{
    if (initialized) {
        return 0;
    }

    if (!adc_is_ready_dt(&ldr_adc)) {
        return -ENODEV;
    }

    int ret = adc_channel_setup_dt(&ldr_adc);

    if (ret != 0) {
        return ret;
    }

    ret = adc_sequence_init_dt(&ldr_adc, &sequence);

    if (ret != 0) {
        return ret;
    }

    if (!gpio_is_ready_dt(&ldr_pwr)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(
        &ldr_pwr,
        GPIO_OUTPUT_INACTIVE
    );

    if (ret != 0) {
        return ret;
    }

    initialized = true;

    return 0;
}

int ldr_service_read(void)
{
    if (!initialized) {
        return -EACCES;
    }

    event_t event = {0};
    int ret;

    LDR_PM_LOCK();

    ret = gpio_pin_set_dt(&ldr_pwr, 1);

    if (ret == 0) {
        k_msleep(LDR_SETTLE_MS);
        ret = adc_read_dt(&ldr_adc, &sequence);
    }

    int power_off_ret = gpio_pin_set_dt(&ldr_pwr, 0);

    LDR_PM_UNLOCK();

    if (ret == 0 && power_off_ret != 0) {
        ret = power_off_ret;
    }

    if (ret != 0) {
        telemetry_flag_set(FLAG_LDR_INVALID);
        return ret;
    }

    int32_t value = sample < 0 ? 0 : sample;

    event.type = EVENT_ADC_READING;
    event.adc_raw = (uint16_t)value;

    event.brightness_pct =
        (uint16_t)MIN(
            (value * 100) / ADC_FULL_SCALE,
            100
        );

    telemetry_set_light(
        ldr_raw_to_dpct(event.adc_raw)
    );

#if LDR_CALIBRATED
    if (event.adc_raw < LDR_RAW_DARK ||
        event.adc_raw > LDR_RAW_BRIGHT) {

        telemetry_flag_set(FLAG_LDR_INVALID);
    }

    telemetry_flag_clear(FLAG_CAL_PENDING);
#else
    if (event.adc_raw < 0 ||
        event.adc_raw > 4095) {

        telemetry_flag_set(FLAG_LDR_INVALID);
    }
#endif


    return events_send(&event);
}
