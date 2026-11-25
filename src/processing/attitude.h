#ifndef ATTITUDE_H
#define ATTITUDE_H

#include <stdbool.h>

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    bool initialized;
} attitude_t;

// Reset the attitude estimation to an uninitialized state
void attitude_reset(void);

/* Update the attidude estimation based on new accelerometer and gyroscope data.
*  Returns: 0 on success, -EINVAL on failure, -EAGAIN if the attitude estimation is not initialized yet.
*/

int attitude_update(float ax, float ay, float az,
                    float gx, float gy, float gz,
                    float dt_s,
                    attitude_t *out);

#endif