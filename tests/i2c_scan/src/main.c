#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#define SI7021_I2C_SLAVEADDR 0x40
#define SI7021_MEASURE_TEMP 0xE3
#define SI7021_MEASURE_RH 0xE5

#define MPU6050_I2C_SLAVEADDR 0x68
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B 

#define I2C_NODE DT_ALIAS(i2c_bus)

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

int mpu6050_read_accel(const struct device *i2c_dev, int16_t *accelx, int16_t *accely, int16_t *accelz) {
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t data[6];

    if (i2c_write(i2c_dev, &reg, 1, MPU6050_I2C_SLAVEADDR) != 0) {
        printk("Failed to request accelerometer data\n");
        return -1;
    }

    k_msleep(10);

    if (i2c_read(i2c_dev, data, 6, MPU6050_I2C_SLAVEADDR) != 0) {
        printk("Failed to read accelerometer data\n");
        return -1;
    }

    *accelx = (data[0] << 8) | data[1];
    *accely = (data[2] << 8) | data[3];
    *accelz = (data[4] << 8) | data[5];

    return 0;

}

int main(void)
{
    const struct device *bus = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(bus)) {
        printk("I2C bus not ready\n");
        return 0;
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