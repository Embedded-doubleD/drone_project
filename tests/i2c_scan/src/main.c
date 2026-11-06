#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
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

    if (i2c_write(i2c_dev, &cmd, 1, SI7021_I2C_SLAVEADDR) != 0) {
        printk("Failed to write temperature command\n");
        return -1;
    }

    k_msleep(20); 
    
    if (i2c_read(i2c_dev, data, 2, SI7021_I2C_SLAVEADDR) != 0) {
        printk("Failed to read temperature data\n");
        return -1; 
    }

    int16_t temp_raw = (data[0] << 8) | data[1];
    *temperature = (float)temp_raw / 65536.0 * 175.72 - 46.85;

    return 0; 
}

int si7021_read_humidity(const struct device *i2c_dev, float *humidity) {
    uint8_t cmd = SI7021_MEASURE_RH;
    uint8_t data[2];

    if (i2c_write(i2c_dev, &cmd, 1, SI7021_I2C_SLAVEADDR) != 0) {
        printk("Failed to write humidity command\n");
        return -1;
    }

    k_msleep(25);

    if (i2c_read(i2c_dev, data, 2, SI7021_I2C_SLAVEADDR) != 0) {
        printk("Failed to read humidity data\n");
        return -1;
    }

    uint16_t rh_code = (data[0] << 8) | data[1];
    *humidity = ((125.0f * rh_code) / 65536.0f) - 6.0f;

    return 0;
}

int mpu6050_init(const struct device *i2c_dev) {
    uint8_t data[2] = {MPU6050_REG_PWR_MGMT_1, 0x00};

    if (i2c_write(i2c_dev, data, 2, MPU6050_I2C_SLAVEADDR) != 0) {
        printk("Failed to wake up MPU6050\n");
        return -1;
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

    int ret = i2c_write_read(i2c_dev,
                             MPU6050_I2C_SLAVEADDR,
                             &reg,
                             sizeof(reg),
                             data,
                             sizeof(data));

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

    if (mpu6050_check_id(bus) != 0) {
        printk("MPU6050 not found or invalid ID\n");
    }

    mpu6050_init(bus);

    while (1) {
        int16_t accelx, accely, accelz;
        float temperature, humidity;

        if (mpu6050_read_accel(bus, &accelx, &accely, &accelz) == 0) {
            printk("Accelerometer: X=%d, Y=%d, Z=%d\n", accelx, accely, accelz);
        } else {
            printk("Failed to read accelerometer data\n");
        }

        if (si7021_read_temperature(bus, &temperature) == 0) {
            printk("Temperature: %.2f C\n", temperature);
        } else {
            printk("Failed to read temperature data\n");
        }

        if (si7021_read_humidity(bus, &humidity) == 0) {
            printk("Humidity: %.2f %%\n", humidity);
        } else {
            printk("Failed to read humidity data\n");
        }

        k_msleep(2000);
    }
    return 0;
}