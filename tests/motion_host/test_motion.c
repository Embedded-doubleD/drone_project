#include <math.h>
#include <stdio.h>
#include <errno.h>
#include "motion.h"

#define G 9.80665f
static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FALLO: %s\n", msg); fails++; } } while (0)

static motion_t run(int n, float ax, float ay, float az, float gz,
                    float roll, float pitch, float yaw)
{
    motion_t m = {0};
    for (int i = 0; i < n; i++) {
        motion_update(ax, ay, az, 0, 0, gz, roll, pitch, yaw, 0.02f, &m);
    }
    return m;
}

int main(void)
{
    motion_t m;
    const float d2r = 3.14159265f / 180.0f;

    motion_reset();
    m = run(500, 0, 0, G, 0, 0, 0, 0);
    CHECK(fabsf(m.z_m) < 0.01f && fabsf(m.vz_ms) < 0.01f, "reposo plano: Z debe quedarse en 0");

    motion_reset();
    m = run(500, 0, G * sinf(30 * d2r), G * cosf(30 * d2r), 0, 30, 0, 0);
    CHECK(fabsf(m.z_m) < 0.01f, "reposo con roll 30: la gravedad debe compensarse");

    motion_reset();
    m = run(50, 0, 0, G + 1.0f, 0, 0, 0, 0);
    CHECK(fabsf(m.vz_ms - 1.0f) < 0.02f, "a=1 m/s2 durante 1 s: vz = 1 m/s");
    CHECK(fabsf(m.z_m - 0.5f) < 0.03f, "a=1 m/s2 durante 1 s: z = 0,5 m");

    for (int i = 0; i < 30; i++) {
        motion_update(0, 0, G, 0, 0, 0, 0, 0, 0, 0.02f, &m);
    }
    CHECK(m.stationary && m.vz_ms == 0.0f, "tras 0,5 s quieto la velocidad vuelve a 0");

    motion_reset();
    m = run(50, 1.0f, 0, G, 0, 0, 0, 90.0f);
    CHECK(m.y_m > 0.4f && fabsf(m.x_m) < 0.05f, "yaw 90: aceleracion en X del sensor va a Y local");

    CHECK(motion_update(0, 0, G, 0, 0, 0, 0, 0, 0, 0.0f, &m) == -EINVAL, "dt=0 invalido");
    CHECK(motion_update(NAN, 0, G, 0, 0, 0, 0, 0, 0, 0.02f, &m) == -EINVAL, "NaN invalido");
    CHECK(motion_update(0, 0, G, 0, 0, 0, 0, 0, 0, 0.02f, NULL) == -EINVAL, "out NULL invalido");

    printf(fails ? "FALLOS: %d\n" : "OK (0 fallos)\n", fails);
    return fails != 0;
}