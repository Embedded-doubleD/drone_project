
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>

#include "temp_hum_service.h"
#include "telemetry_data.h"
#include "config.h"

#define I2C_NODE                DT_ALIAS(i2c_bus)

#define SI7021_ADDR             0x40
#define SI7021_CMD_MEASURE_RH   0xF5
#define SI7021_CMD_READ_TEMP    0xE0

#define SI7021_CONVERSION_MS    20
#define SI7021_READ_RETRIES     5
#define SI7021_RETRY_DELAY_MS   2

static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_NODE);

static bool initialized = false;


/* Lee una palabra de 16 bits del Si7021. */
static int si7021_read_word(uint8_t command, uint16_t *value)
{
    uint8_t data[2];
    int ret;

    ret = i2c_write(i2c_dev, &command, 1, SI7021_ADDR);
    if (ret < 0) {
        return ret;
    }

    ret = i2c_read(i2c_dev, data, sizeof(data), SI7021_ADDR);
    if (ret < 0) {
        return ret;
    }

    *value = ((uint16_t)data[0] << 8) | data[1];
    *value &= 0xFFFCu;

    return 0;
}


/* Lee humedad y temperatura asociada a la misma conversión. */
static int si7021_read_environment(int32_t *temp_dc, int32_t *hum_dpct)
{
    uint8_t command = SI7021_CMD_MEASURE_RH;
    uint8_t data[2];
    uint16_t raw_rh;
    uint16_t raw_temp;
    int ret;

    /* Iniciar conversión de humedad. */
    ret = i2c_write(i2c_dev, &command, 1, SI7021_ADDR);
    if (ret < 0) {
        return ret;
    }

    k_msleep(SI7021_CONVERSION_MS);

    /* Leer humedad, reintentando si el sensor todavía no responde. */
    for (int i = 0; i < SI7021_READ_RETRIES; i++) {
        ret = i2c_read(i2c_dev, data, sizeof(data), SI7021_ADDR);

        if (ret == 0) {
            break;
        }

        k_msleep(SI7021_RETRY_DELAY_MS);
    }

    if (ret < 0) {
        return ret;
    }

    raw_rh = ((uint16_t)data[0] << 8) | data[1];
    raw_rh &= 0xFFFCu;

    /* Leer temperatura de la conversión de humedad anterior. */
    ret = si7021_read_word(SI7021_CMD_READ_TEMP, &raw_temp);
    if (ret < 0) {
        return ret;
    }

    /* Convertir a décimas de unidad. */
    *hum_dpct = (int32_t)(
        (1250.0f * raw_rh / 65536.0f) - 60.0f
    );

    *temp_dc = (int32_t)(
        (1757.2f * raw_temp / 65536.0f) - 468.5f
    );

    /* Limitar humedad al intervalo 0-100 %. */
    if (*hum_dpct < 0) {
        *hum_dpct = 0;
    } else if (*hum_dpct > 1000) {
        *hum_dpct = 1000;
    }

    return 0;
}


/* Inicializa el servicio del Si7021. */
int temp_hum_init(void)
{
    if (initialized) {
        return 0;
    }

    if (!device_is_ready(i2c_dev)) {
        return -ENODEV;
    }

    initialized = true;

    return 0;
}


/* Realiza una adquisición síncrona de temperatura y humedad. */
int temp_hum_read(int32_t *temp_dc, int32_t *hum_dpct)
{
    int ret;

    if (!initialized) {
        return -EACCES;
    }

    if (temp_dc == NULL || hum_dpct == NULL) {
        return -EINVAL;
    }

    ret = si7021_read_environment(temp_dc, hum_dpct);

    if (ret != 0) {
        telemetry_flag_set(FLAG_ENV_INVALID);
        return ret;
    }

    telemetry_set_env(*temp_dc, *hum_dpct);
    telemetry_flag_clear(FLAG_ENV_INVALID);

    return 0;
}
