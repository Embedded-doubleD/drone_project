#include "imu_interrupt.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include <errno.h>

/* MPU6050 interrupt GPIO */
#define IMU_INT_PIN 6

static struct gpio_callback imu_int_callback;

static const struct device *imu_gpio_port =
    DEVICE_DT_GET(DT_NODELABEL(gpioa));

/* One pending event is enough: we only need the latest sample. */
K_SEM_DEFINE(imu_data_ready_sem, 0, 1);

/*
 * GPIO interrupt handler.
 * Keep the ISR short: do not perform I2C operations here.
 */
static void imu_gpio_isr(const struct device *dev,
                         struct gpio_callback *cb,
                         uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_sem_give(&imu_data_ready_sem);
}

/*
 * Configure the MCU GPIO interrupt callback.
 */
int imu_interrupt_init(void)
{
    int ret;

    if (!device_is_ready(imu_gpio_port)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure(imu_gpio_port,
                             IMU_INT_PIN,
                             GPIO_INPUT);

    if (ret < 0) {
        return ret;
    }

    gpio_init_callback(&imu_int_callback,
                       imu_gpio_isr,
                       BIT(IMU_INT_PIN));

    ret = gpio_add_callback(imu_gpio_port,
                            &imu_int_callback);

    if (ret < 0) {
        return ret;
    }

    return 0;
}

/*
 * Enable the MCU-side rising-edge interrupt.
 */
int imu_interrupt_enable(void)
{
    return gpio_pin_interrupt_configure(
        imu_gpio_port,
        IMU_INT_PIN,
        GPIO_INT_EDGE_RISING
    );
}

int imu_interrupt_wait(k_timeout_t timeout)
{
    return k_sem_take(&imu_data_ready_sem, timeout);
}

void imu_interrupt_reset(void)
{
    k_sem_reset(&imu_data_ready_sem);
}
