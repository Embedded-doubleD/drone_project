
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include <errno.h>

#include "imu_service.h"
#include "telemetry_data.h"

/* MPU6050 interrupt GPIO */
#define IMU_INT_PIN 6

#define IMU_STACK_SIZE  1024
#define IMU_PRIORITY    6

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
 * Configure the MCU GPIO interrupt.
 */
static int imu_gpio_interrupt_init(void)
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
 * IMU acquisition thread.
 * Sleeps until the MPU6050 signals Data Ready.
 */
static void imu_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    imu_data_t imu;
    int ret;

    /* Initialize GPIO first, while sensor interrupts are disabled. */
    ret = imu_gpio_interrupt_init();

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    /* Initialize MPU6050 */
    while ((ret = imu_init()) < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        k_msleep(1000);
    }

    /* First enable the MCU GPIO interrupt */
    ret = gpio_pin_interrupt_configure(
        imu_gpio_port,
        IMU_INT_PIN,
        GPIO_INT_EDGE_RISING
    );

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    /* Then enable Data Ready interrupt on the MPU6050 */
    ret = imu_enable_data_ready_interrupt();

    if (ret < 0) {
        telemetry_flag_set(FLAG_IMU_INVALID);
        return;
    }

    while (1) {

        /* Block until the GPIO ISR gives the semaphore. */
        ret = k_sem_take(&imu_data_ready_sem, K_FOREVER);

        if (ret < 0) {
            telemetry_flag_set(FLAG_IMU_INVALID);
            continue;
        }

        ret = imu_read(&imu);

        if (ret == 0) {

            telemetry_set_imu(
                (int32_t)(imu.accel_x * 100.0f),
                (int32_t)(imu.accel_y * 100.0f),
                (int32_t)(imu.accel_z * 100.0f),

                (int32_t)(imu.gyro_x * 10.0f),
                (int32_t)(imu.gyro_y * 10.0f),
                (int32_t)(imu.gyro_z * 10.0f)
            );

        } else {
            telemetry_flag_set(FLAG_IMU_INVALID);
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
