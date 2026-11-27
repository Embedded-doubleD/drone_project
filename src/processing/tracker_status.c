#include <zephyr/kernel.h>

#include <math.h>
#include <stdbool.h>

#include "tracker_status.h"
#include "telemetry_data.h"

/* Physical constants */
#define GRAVITY_M_S2                9.80665f

/* Motion detection thresholds */
#define FLYING_GYRO_THRESHOLD       8.0f
#define FLYING_ACCEL_DEVIATION      1.5f

#define IDLE_GYRO_THRESHOLD         5.0f
#define IDLE_ACCEL_DEVIATION        0.8f

/* Critical conditions */
#define CRITICAL_GYRO_THRESHOLD     220.0f
#define CRITICAL_GYRO_TIME_MS       300
/* State transition persistence */
#define FLYING_CONFIRM_MS           1000
#define IDLE_CONFIRM_MS             3000
#define PROBLEM_RECOVERY_MS         3000


static tracker_state_t current_state = TRACKER_IDLE;

static int64_t movement_start_ms;
static int64_t idle_start_ms;
static int64_t critical_start_ms;
static int64_t invalid_start_ms;
static int64_t problem_recovery_start_ms;
static bool problem_latched;
static bool gps_communication_error;

void tracker_status_init(void)
{
    current_state = TRACKER_IDLE;

    movement_start_ms = 0;
    idle_start_ms = 0;
    critical_start_ms = 0;
    invalid_start_ms = 0;
    problem_recovery_start_ms = 0;

    problem_latched = false;
    gps_communication_error = false;

    telemetry_set_state(TRACKER_IDLE);
}

static float acceleration_magnitude(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

static float angular_velocity_magnitude(float gx, float gy, float gz)
{
    return sqrtf(gx * gx + gy * gy + gz * gz);
}

static void set_state(tracker_state_t state)
{
    if (current_state != state) {
        current_state = state;
        telemetry_set_state(state);
    }
}

void tracker_status_imu_error(void)
{
    telemetry_flag_set(FLAG_IMU_INVALID);

    /*
     * An IMU failure is a safety-critical condition.  Enter PROBLEM on the
     * first missed/failed sample instead of allowing stale motion data to
     * keep the tracker in FLYING.
     */
    problem_latched = true;
    invalid_start_ms = k_uptime_get();
    problem_recovery_start_ms = 0;
    set_state(TRACKER_PROBLEM);
}

void tracker_status_gps_communication_error(void)
{
    telemetry_flag_set(FLAG_GPS_INVALID);
    gps_communication_error = true;
    problem_latched = true;
    problem_recovery_start_ms = 0;
    set_state(TRACKER_PROBLEM);
}

void tracker_status_gps_communication_recovered(void)
{
    gps_communication_error = false;
}

void tracker_status_update(float ax, float ay, float az,
                           float gx, float gy, float gz,
                           uint16_t flags)
{
    int64_t now = k_uptime_get();

    float acceleration;
    float angular_velocity;
    float acceleration_deviation;

    bool movement_detected;
    bool idle_detected;
    bool critical_condition;

    /*
     * This function is called only after a valid IMU sample.
     * Therefore, a successful read ends the current invalid interval.
     */
    invalid_start_ms = 0;

    /* An impact flag has immediate priority. */
    if ((flags & FLAG_IMPACT) != 0U) {
        problem_latched = true;
    }

    acceleration = acceleration_magnitude(ax, ay, az);
    angular_velocity = angular_velocity_magnitude(gx, gy, gz);

    acceleration_deviation = fabsf(acceleration - GRAVITY_M_S2);

    /* Critical angular velocity must persist. */
    critical_condition =
        (angular_velocity >= CRITICAL_GYRO_THRESHOLD);

    if (critical_condition) {
        if (critical_start_ms == 0) {
            critical_start_ms = now;
        }

        if ((now - critical_start_ms) >= CRITICAL_GYRO_TIME_MS) {
            problem_latched = true;
        }
    } else {
        critical_start_ms = 0;
    }

    /*
     * PROBLEM has the highest priority.
     * Require a healthy recovery interval before clearing it.
     */
    if (problem_latched) {
        bool healthy =
            ((flags & (FLAG_IMU_INVALID | FLAG_IMPACT)) == 0U) &&
            !gps_communication_error &&
            !critical_condition;

        if (healthy) {
            if (problem_recovery_start_ms == 0) {
                problem_recovery_start_ms = now;
            }

            if ((now - problem_recovery_start_ms) >=
                PROBLEM_RECOVERY_MS) {
                problem_latched = false;
                problem_recovery_start_ms = 0;
            }
        } else {
            problem_recovery_start_ms = 0;
        }

        if (problem_latched) {
            set_state(TRACKER_PROBLEM);
            return;
        }
    }

    /* Motion detection with separate entry and exit thresholds. */
    movement_detected =
        (angular_velocity >= FLYING_GYRO_THRESHOLD) ||
        (acceleration_deviation >= FLYING_ACCEL_DEVIATION);

    idle_detected =
        (angular_velocity <= IDLE_GYRO_THRESHOLD) &&
        (acceleration_deviation <= IDLE_ACCEL_DEVIATION);

    if (movement_detected) {
        idle_start_ms = 0;

        if (movement_start_ms == 0) {
            movement_start_ms = now;
        }

        if ((now - movement_start_ms) >= FLYING_CONFIRM_MS) {
            set_state(TRACKER_FLYING);
        }
    } else {
        movement_start_ms = 0;
    }

    if (idle_detected) {
        if (idle_start_ms == 0) {
            idle_start_ms = now;
        }

        if ((now - idle_start_ms) >= IDLE_CONFIRM_MS) {
            set_state(TRACKER_IDLE);
        }
    } else {
        idle_start_ms = 0;
    }
}
