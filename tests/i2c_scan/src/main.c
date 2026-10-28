#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#define I2C_NODE DT_ALIAS(i2c_bus)

int main(void)
{
    const struct device *bus = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(bus)) {
        printk("I2C bus not ready\n");
        return 0;
    }

    while (1) {
        int found = 0;
        printk("Scanning I2C...\n");

        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            uint8_t dummy;
            if (i2c_read(bus, &dummy, 1, addr) == 0) {
                printk("  device at 0x%02X\n", addr);
                found++;
            }
        }

        printk("Done: %d device(s)\n\n", found);
        k_msleep(5000);
    }
    return 0;
}