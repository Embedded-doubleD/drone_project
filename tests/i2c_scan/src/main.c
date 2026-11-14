#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>
#include <errno.h>

#define SI7021_I2C_SLAVEADDR 0x40
#define SI7021_MEASURE_TEMP 0xF3
#define SI7021_MEASURE_RH   0xF5

#define MPU6050_I2C_SLAVEADDR 0x68
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B 
#define MPU6050_REG_WHO_AM_I  0x75
#define MPU6050_WHO_AM_I_VALUE 0x68

#define I2C_NODE DT_ALIAS(i2c_bus)


int mpu6050_check_id(const struct device *i2c_dev)
{
    uint8_t reg = MPU6050_REG_WHO_AM_I;
    uint8_t id;

    int ret = i2c_write_read(i2c_dev,
                             MPU6050_I2C_SLAVEADDR,
                             &reg,
                             sizeof(reg),
                             &id,
                             sizeof(id));

    if (ret < 0) {
        return ret;
    }

    if (id != MPU6050_WHO_AM_I_VALUE) {
        printk("MPU6050 invalid ID: 0x%02X\n", id);
        return -ENODEV;
    }

    printk("MPU6050 WHO_AM_I PASS\n");

    return 0;
}

int si7021_read_temperature(const struct device *i2c_dev, float *temperature) {
    uint8_t cmd = SI7021_MEASURE_TEMP;
    uint8_t data[2];
    int ret;

    ret = i2c_write(i2c_dev, &cmd, 1, SI7021_I2C_SLAVEADDR);
    if (ret < 0) {
        printk("Failed to write temperature command\n");
        return ret;
    }

    k_msleep(20); 
    
    ret = i2c_read(i2c_dev, data, 2, SI7021_I2C_SLAVEADDR);
    if (ret < 0) {
        printk("Failed to read temperature data\n");
        return ret;
    }

    int16_t temp_raw = (data[0] << 8) | data[1];
    *temperature = (float)temp_raw / 65536.0 * 175.72 - 46.85;

    return 0; 
}

int si7021_read_humidity(const struct device *i2c_dev, float *humidity) {
    uint8_t cmd = SI7021_MEASURE_RH;
    uint8_t data[2];
    int ret;

    ret = i2c_write(i2c_dev, &cmd, 1, SI7021_I2C_SLAVEADDR);
    if (ret < 0) {
        printk("Failed to write humidity command\n");
        return ret;
    }

    k_msleep(25);

    ret = i2c_read(i2c_dev, data, 2, SI7021_I2C_SLAVEADDR);
    if (ret < 0) {
        printk("Failed to read humidity data\n");
        return ret;
    }

    uint16_t rh_code = (data[0] << 8) | data[1];
    *humidity = ((125.0f * rh_code) / 65536.0f) - 6.0f;

    return 0;
}

int mpu6050_init(const struct device *i2c_dev)
{
    uint8_t data[2] = {
        MPU6050_REG_PWR_MGMT_1,
        0x00
    };

    int ret = i2c_write(
        i2c_dev,
        data,
        sizeof(data),
        MPU6050_I2C_SLAVEADDR
    );

    if (ret < 0) {
        printk("Failed to wake up MPU6050: %d\n", ret);
        return ret;
    }

    k_msleep(50);

    return 0;
}

int mpu6050_read_accel(const struct device *i2c_dev,
                       int16_t *accelx,
                       int16_t *accely,
                       int16_t *accelz)
{
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t data[6];
    int ret;

    ret = i2c_write_read(i2c_dev,
                          MPU6050_I2C_SLAVEADDR,
                          &reg,
                          1,
                          data,
                          sizeof(data)
    );

    if (ret < 0) {
        printk("MPU6050 I2C error: %d\n", ret);
        return ret;
    }

    *accelx = (int16_t)((data[0] << 8) | data[1]);
    *accely = (int16_t)((data[2] << 8) | data[3]);
    *accelz = (int16_t)((data[4] << 8) | data[5]);

    return 0;
}

int main(void)
{
    const struct device *bus = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(bus)) {
        printk("I2C bus not ready\n");
        return 0;
    }

    bool mpu_ready = false;

    while (1) {
        int16_t accelx, accely, accelz;
        float temperature, humidity;
        int ret;

        /*
         * MPU6050 detection and recovery
         */
        if (!mpu_ready) {
            ret = mpu6050_check_id(bus);

            if (ret == 0) {
                ret = mpu6050_init(bus);

                if (ret == 0) {
                    mpu_ready = true;
                    printk("MPU6050 initialized successfully\n");
                } else {
                    printk("MPU6050 initialization failed: %d\n", ret);
                }
            } else {
                printk("MPU6050 not available: %d\n", ret);
            }
        }

        /*
         * Accelerometer
         */
        if (mpu_ready) {
            ret = mpu6050_read_accel(
                bus,
                &accelx,
                &accely,
                &accelz
            );

            if (ret == 0) {
                printk(
                    "Accelerometer: X=%d, Y=%d, Z=%d\n",
                    accelx,
                    accely,
                    accelz
                );
            } else {
                printk("MPU6050 read failed: %d\n", ret);
                mpu_ready = false;
            }
        }

        /*
         * Si7021 temperature
         */
        ret = si7021_read_temperature(bus, &temperature);

        if (ret == 0) {
            printk("Temperature: %.2f C\n", (double)temperature);
        } else {
            printk("Temperature read failed: %d\n", ret);
        }

        /*
         * Si7021 humidity
         */
        ret = si7021_read_humidity(bus, &humidity);

        if (ret == 0) {
            printk("Humidity: %.2f %%\n", (double)humidity);
        } else {
            printk("Humidity read failed: %d\n", ret);
        }

        k_msleep(2000);
    }

    return 0;
}