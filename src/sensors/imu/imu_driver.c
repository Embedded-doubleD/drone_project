
#include "imu_driver.h"
#include "config.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* MPU6050 I2C configuration */
#define MPU6050_I2C_ADDR          0x68

/* MPU6050 registers */
#define MPU6050_REG_SMPLRT_DIV     0x19
#define MPU6050_REG_CONFIG         0x1A
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_ACCEL_XOUT_H   0x3B
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_WHO_AM_I       0x75

/* MPU6050 interrupt configuration registers */
#define MPU6050_REG_INT_PIN_CFG  0x37
#define MPU6050_REG_INT_ENABLE   0x38
#define MPU6050_REG_INT_STATUS   0x3A

#define MPU6050_WHO_AM_I_VALUE     0x68

/* MPU6050 sensitivities */
#define MPU6050_ACCEL_SENSITIVITY  16384.0f
#define MPU6050_GYRO_SENSITIVITY   131.0f

#define GRAVITY                    9.80665f

#define I2C_NODE DT_ALIAS(i2c_bus)

static bool is_imu_initialized = false;


/* I2C device */
static const struct device *i2c_dev;

/*
 * Converts two bytes (MSB and LSB) into a signed 16-bit value.
 */
static int16_t imu_get_int16(uint8_t msb, uint8_t lsb)
{
    uint16_t value = ((uint16_t)msb << 8) | (uint16_t)lsb;

    return (int16_t)value;
}

/*
 * Writes a register in the MPU6050.
 */
static int imu_write_register(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };

    return i2c_write(i2c_dev,
                     data,
                     sizeof(data),
                     MPU6050_I2C_ADDR);
}

/*
 * Reads the MPU6050 identity register.
 */
static int imu_check_id(void)
{
    uint8_t reg = MPU6050_REG_WHO_AM_I;
    uint8_t id;

    int ret = i2c_write_read(i2c_dev,
                             MPU6050_I2C_ADDR,
                             &reg,
                             sizeof(reg),
                             &id,
                             sizeof(id));

    if (ret < 0) {
        return ret;
    }

    if (id != MPU6050_WHO_AM_I_VALUE) {
        return -ENODEV;
    }

    return 0;
}

/*
 * Initializes the MPU6050.
 */
int imu_init(void)
{
    int ret;

    is_imu_initialized = false;

    /* Get the I2C bus from DeviceTree */
    i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        i2c_dev = NULL;
        return -ENODEV;
    }

    /* Check sensor identity */
    ret = imu_check_id();

    if (ret < 0) {
        return ret;
    }

    /* Wake up the sensor */
    ret = imu_write_register(MPU6050_REG_PWR_MGMT_1, 0x00);

    if (ret < 0) {
        return ret;
    }

    k_msleep(50);

    /* Sample rate divider: 50 Hz (1 kHz / (1 + 19)) */
    ret = imu_write_register(MPU6050_REG_SMPLRT_DIV, 19);

    if (ret < 0) {
        return ret;
    }

    /* Digital low-pass filter */
    ret = imu_write_register(MPU6050_REG_CONFIG, 0x03);

    if (ret < 0) {
        return ret;
    }

    /* Gyroscope range: +/- 250 degrees/second */
    ret = imu_write_register(MPU6050_REG_GYRO_CONFIG, 0x00);

    if (ret < 0) {
        return ret;
    }

    /* Accelerometer range: +/- 2g */
    ret = imu_write_register(MPU6050_REG_ACCEL_CONFIG, 0x00);

    if (ret < 0) {
        return ret;
    }

    k_msleep(50);


    is_imu_initialized = true;
    return 0;
}

/*
 * Reads raw accelerometer and gyroscope measurements.
 *
 * A single 14-byte read obtains:
 * - 6 bytes of accelerometer data
 * - 2 bytes of temperature data (not used)
 * - 6 bytes of gyroscope data
 */
int imu_read_raw(imu_raw_data_t *data)
{
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buffer[14];

    int ret;

    if (data == NULL) {
        return -EINVAL;
    }

    if (i2c_dev == NULL || !device_is_ready(i2c_dev) || !is_imu_initialized) {
        return -ENODEV;
    }

    ret = i2c_write_read(
        i2c_dev,
        MPU6050_I2C_ADDR,
        &reg,
        sizeof(reg),
        buffer,
        sizeof(buffer)
    );

    if (ret < 0) {
        /*
        * Try to clear the latched interrupt even if
        * the measurement read failed.
        */
        uint8_t status_reg = MPU6050_REG_INT_STATUS;
        uint8_t status;

        int status_ret = i2c_write_read(
            i2c_dev,
            MPU6050_I2C_ADDR,
            &status_reg,
            sizeof(status_reg),
            &status,
            sizeof(status)
        );

        if (status_ret < 0) {
            return ret;
        }

        return ret;
    }

    /* Accelerometer */
    data->accel_x = imu_get_int16(buffer[0], buffer[1]);
    data->accel_y = imu_get_int16(buffer[2], buffer[3]);
    data->accel_z = imu_get_int16(buffer[4], buffer[5]);

    /* Bytes 6 and 7 contain the temperature measurement */

    /* Gyroscope */
    data->gyro_x = imu_get_int16(buffer[8], buffer[9]);
    data->gyro_y = imu_get_int16(buffer[10], buffer[11]);
    data->gyro_z = imu_get_int16(buffer[12], buffer[13]);


    // Clear INT status.
    uint8_t status_reg = MPU6050_REG_INT_STATUS;
    uint8_t status;

    ret = i2c_write_read(
        i2c_dev,
        MPU6050_I2C_ADDR,
        &status_reg,
        sizeof(status_reg),
        &status,
        sizeof(status)
    );

    if (ret < 0) {
        return ret;
    }

    return 0;
}

/*
 * Reads measurements and converts them into physical units.
 *
 * Accelerometer: m/s^2
 * Gyroscope: degrees/second
 */
int imu_read(imu_data_t *data)
{
    imu_raw_data_t raw;

    int ret;

    if (data == NULL) {
        return -EINVAL;
    }

    ret = imu_read_raw(&raw);

    if (ret < 0) {
        return ret;
    }

    /* Convert accelerometer values to m/s^2 */
    data->accel_x =
        ((float)raw.accel_x / MPU6050_ACCEL_SENSITIVITY) * GRAVITY;

    data->accel_y =
        ((float)raw.accel_y / MPU6050_ACCEL_SENSITIVITY) * GRAVITY;

    data->accel_z =
        ((float)raw.accel_z / MPU6050_ACCEL_SENSITIVITY) * GRAVITY;

    /* Convert gyroscope values to degrees/second */
    data->gyro_x =
        (float)raw.gyro_x / MPU6050_GYRO_SENSITIVITY;

    data->gyro_y =
        (float)raw.gyro_y / MPU6050_GYRO_SENSITIVITY;

    data->gyro_z =
        (float)raw.gyro_z / MPU6050_GYRO_SENSITIVITY;

    return 0;
}


int imu_enable_data_ready_interrupt(void)
{
    int ret;

    /*
     * Active high
     * Push-pull
     * Clear interrupt by reading INT_STATUS
     */
    ret = imu_write_register(MPU6050_REG_INT_PIN_CFG, 0x00);

    if (ret < 0) {
        return ret;
    }

    /* Enable Data Ready interrupt */
    ret = imu_write_register(MPU6050_REG_INT_ENABLE, 0x01);

    if (ret < 0) {
        return ret;
    }

    return 0;
}
