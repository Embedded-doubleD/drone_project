#include <zephyr/kernel.h>

#include <math.h>
#include <string.h>

#include "statistics.h"

K_MUTEX_DEFINE(statistics_mutex);

typedef struct {
    float sum;
    float min;
    float max;
    unsigned int count;
} accumulator_t;

static accumulator_t light_acc;
static accumulator_t temperature_acc;
static accumulator_t humidity_acc;
static accumulator_t acceleration_acc;
static accumulator_t angular_velocity_acc;

static void accumulator_add(accumulator_t *acc, float value)
{
    if (acc->count == 0) {
        acc->min = value;
        acc->max = value;
    } else {
        if (value < acc->min) {
            acc->min = value;
        }

        if (value > acc->max) {
            acc->max = value;
        }
    }

    acc->sum += value;
    acc->count++;
}

static void accumulator_snapshot(const accumulator_t *acc,
                                 statistics_value_t *result)
{
    result->count = acc->count;

    if (acc->count == 0) {
        result->mean = 0.0f;
        result->min = 0.0f;
        result->max = 0.0f;
        return;
    }

    result->mean = acc->sum / (float)acc->count;
    result->min = acc->min;
    result->max = acc->max;
}

void statistics_reset(void)
{
    k_mutex_lock(&statistics_mutex, K_FOREVER);

    memset(&light_acc, 0, sizeof(light_acc));
    memset(&temperature_acc, 0, sizeof(temperature_acc));
    memset(&humidity_acc, 0, sizeof(humidity_acc));
    memset(&acceleration_acc, 0, sizeof(acceleration_acc));
    memset(&angular_velocity_acc, 0, sizeof(angular_velocity_acc));

    k_mutex_unlock(&statistics_mutex);
}

void statistics_add_light(float value)
{
    k_mutex_lock(&statistics_mutex, K_FOREVER);
    accumulator_add(&light_acc, value);
    k_mutex_unlock(&statistics_mutex);
}

void statistics_add_environment(float temperature, float humidity)
{
    k_mutex_lock(&statistics_mutex, K_FOREVER);

    accumulator_add(&temperature_acc, temperature);
    accumulator_add(&humidity_acc, humidity);

    k_mutex_unlock(&statistics_mutex);
}

void statistics_add_imu(float ax, float ay, float az,
                        float gx, float gy, float gz)
{
    float acceleration =
        sqrtf(ax * ax + ay * ay + az * az);

    float angular_velocity =
        sqrtf(gx * gx + gy * gy + gz * gz);

    k_mutex_lock(&statistics_mutex, K_FOREVER);
    
    accumulator_add(&acceleration_acc, acceleration);
    accumulator_add(&angular_velocity_acc, angular_velocity);

    k_mutex_unlock(&statistics_mutex);
}

void statistics_take_snapshot(statistics_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    k_mutex_lock(&statistics_mutex, K_FOREVER);

    accumulator_snapshot(&light_acc, &snapshot->light);
    accumulator_snapshot(&temperature_acc, &snapshot->temperature);
    accumulator_snapshot(&humidity_acc, &snapshot->humidity);
    accumulator_snapshot(&acceleration_acc, &snapshot->acceleration);
    accumulator_snapshot(&angular_velocity_acc,
                         &snapshot->angular_velocity);

    memset(&light_acc, 0, sizeof(light_acc));
    memset(&temperature_acc, 0, sizeof(temperature_acc));
    memset(&humidity_acc, 0, sizeof(humidity_acc));
    memset(&acceleration_acc, 0, sizeof(acceleration_acc));
    memset(&angular_velocity_acc, 0, sizeof(angular_velocity_acc));

    k_mutex_unlock(&statistics_mutex);
}