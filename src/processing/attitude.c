
#include "attitude.h"

#include <errno.h>
#include <math.h>
#include <stddef.h>

/* Constantes físicas */
#define ATTITUDE_GRAVITY_M_S2  9.80665f
#define ATTITUDE_PI            3.14159265358979323846f

#define ATTITUDE_RAD_TO_DEG    (180.0f / ATTITUDE_PI)

/*
 * Time Constant.
 *
 * 50 Hz:
 * dt = 0.02 s
 * alpha = 0.50 / (0.50 + 0.02) = 0.9615
 */
#define ATTITUDE_FILTER_TAU_S  0.50f

// Confidence interval for accelerometer tilt estimation
#define ATTITUDE_ACCEL_MIN_G   0.75f
#define ATTITUDE_ACCEL_MAX_G   1.25f

/* Maximum allowed interval for integrating the gyroscope */
#define ATTITUDE_MAX_GYRO_DT_S 0.10f

static attitude_t estimate;


/*
 * Function that maintains angles within the interval [-180, 180].
 */
static float wrap_degrees(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }

    while (angle < -180.0f) {
        angle += 360.0f;
    }

    return angle;
}


/*
 * Function that calculates roll and pitch from the accelerometer data.
 *
 * Uses the acceleration as a gravity reference only when its magnitude
 * is sufficiently close to 1 g.
 */
static bool accelerometer_tilt(float ax, float ay, float az,
                               float *roll_deg,
                               float *pitch_deg)
{
    const float norm = sqrtf(
        (ax * ax) +
        (ay * ay) +
        (az * az)
    );

    const float min_norm =
        ATTITUDE_ACCEL_MIN_G * ATTITUDE_GRAVITY_M_S2;

    const float max_norm =
        ATTITUDE_ACCEL_MAX_G * ATTITUDE_GRAVITY_M_S2;

    if ((norm < min_norm) || (norm > max_norm)) {
        return false;
    }

    *roll_deg =
        atan2f(ay, az) * ATTITUDE_RAD_TO_DEG;

    *pitch_deg =
        atan2f(-ax, sqrtf((ay * ay) + (az * az)))
        * ATTITUDE_RAD_TO_DEG;

    return true;
}



void attitude_reset(void)
{
    estimate.roll_deg = 0.0f;
    estimate.pitch_deg = 0.0f;
    estimate.yaw_deg = 0.0f;
    estimate.initialized = false;
}


int attitude_update(float ax, float ay, float az,
                    float gx, float gy, float gz,
                    float dt_s,
                    attitude_t *out)
{
    float accel_roll;
    float accel_pitch;

    if (out == NULL) {
        return -EINVAL;
    }

    // Check data validity: all values must be finite and dt_s must be positive
    if (!isfinite(ax) || !isfinite(ay) || !isfinite(az) ||
        !isfinite(gx) || !isfinite(gy) || !isfinite(gz) ||
        !isfinite(dt_s) || (dt_s <= 0.0f)) {
        return -EINVAL;
    }

    const bool accel_valid = accelerometer_tilt(
        ax, ay, az,
        &accel_roll,
        &accel_pitch
    );

    if (!estimate.initialized) {

        if (!accel_valid) {
            return -EAGAIN;
        }

        estimate.roll_deg = accel_roll;
        estimate.pitch_deg = accel_pitch;
        estimate.yaw_deg = 0.0f;

        estimate.initialized = true;

        *out = estimate;

        return 0;
    }

    /* 1. GYROSCOPE INTEGRATION
     *
     * Angle = angular velocity * time interval.
     *
     * This direct integration of the axes is a
     * valid approximation for our initial tests.
    *
    */
    if (dt_s <= ATTITUDE_MAX_GYRO_DT_S) {

        estimate.roll_deg += gx * dt_s;
        estimate.pitch_deg += gy * dt_s;
        estimate.yaw_deg += gz * dt_s;
    }

    /*
     *2. CORRECTION WITH THE ACCELEROMETER
        * The accelerometer corrects the drift of roll and pitch.
        * It cannot correct yaw. 
     */
    
     if (accel_valid) {

        float alpha;

        // Too long intervals are not reliable for the gyroscope integration, so we ignore it.
        if (dt_s > ATTITUDE_MAX_GYRO_DT_S) {

            alpha = 0.0f;

        } else {

            alpha = ATTITUDE_FILTER_TAU_S /
                    (ATTITUDE_FILTER_TAU_S + dt_s);
        }

        /*
         * Complementary filter:
         *
         * angle = alpha * gyroscope
         *        + (1-alpha) * accelerometer
         */
        estimate.roll_deg =
            (alpha * estimate.roll_deg) +
            ((1.0f - alpha) * accel_roll);

        estimate.pitch_deg =
            (alpha * estimate.pitch_deg) +
            ((1.0f - alpha) * accel_pitch);
    }

    // 3. NORMALIZATION
    estimate.roll_deg = wrap_degrees(estimate.roll_deg);
    estimate.pitch_deg = wrap_degrees(estimate.pitch_deg);
    estimate.yaw_deg = wrap_degrees(estimate.yaw_deg);

    *out = estimate;

    return 0;
}
