#include <stdio.h>
#include <string.h>
#include "simu_frame.h"
#include "telemetry_report.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d; %s\n", __LINE__, #cond); failures++; } } while (0)


static const char *EXPECTED_FRAME =
"$SIMU1,00152,0000123456,1,08,+40.389462,-003.627416,+00628.4,+0012.34,-0003.20,+0015.00,"
"+000.50,-000.10,+001.20,+002.40,-001.10,+045.00,-000.12,+000.04,+009.81,+0000.5,-0001.2,"
"+0000.1,073.5,+024.6,045.2,FLYING__,0000*3F\r\n";

static telemetry_t example(void) {
    telemetry_t t = {0};
    t.gps_fix = 1; t.gps_sats = 8;
    t.lat_e6 = 40389462; t.lon_e6 = -3627416; t.alt_dm = 6284;
    t.x_cm = 1234; t.y_cm = -320; t.z_cm = 1500;
    t.vx_cms = 50; t.vy_cms = -10; t.vz_cms = 120;
    t.roll_cdeg = 240; t.pitch_cdeg = -110; t.yaw_cdeg = 4500;
    t.ax_cms2 = -12; t.ay_cms2 = 4; t.az_cms2 = 981;
    t.gx_ddegs = 5; t.gy_ddegs = -12; t.gz_ddegs = 1;
    t.light_dpct = 735; t.temp_dc = 246; t.hum_dpct = 452;
    t.state = TRACKER_FLYING; t.flags = 0;
    return t;
}

int main(void)
{
    char buf[SIMU_FRAME_BUF];
    telemetry_t t = example();

    /* 1. Ejemplo del enunciado byte a byte */
    int n = simu_frame_build(buf, sizeof buf, 152, 123456, &t);
    CHECK(n == SIMU_FRAME_LEN);
    CHECK(strcmp(buf, EXPECTED_FRAME) == 0);

    /* 2. Valores extremos: la longitud no cambia */
    telemetry_t e = {0};
    e.lat_e6 = -999999999; e.alt_dm = 99999999; e.light_dpct = -5; e.gps_sats = 200;
    e.state = TRACKER_PROBLEM; e.flags = 0x0042;
    n = simu_frame_build(buf, sizeof buf, 65535, 4294967295u, &e);
    CHECK(n == SIMU_FRAME_LEN);
    CHECK(buf[n - 2] == '\r' && buf[n - 1] == '\n');

    /* 3. Todo a cero (arranque sin datos): la trama sigue completa */
    telemetry_t z = {0};
    n = simu_frame_build(buf, sizeof buf, 0, 0, &z);
    CHECK(n == SIMU_FRAME_LEN);
    {
        const char *prefix = "$SIMU1,00000,0000000000,0,00,+00.000000,+000.000000,+00000.0,";
        CHECK(strncmp(buf, prefix, strlen(prefix)) == 0);
    }

    /* 4. Buffer corto */
    char small[50];
    CHECK(simu_frame_build(small, sizeof small, 0, 0, &z) < 0);

    /* 5. Informe legible: contiene los datos del ejemplo con el formato del enunciado */
    char rep[REPORT_BUF];
    int r = report_format(rep, sizeof rep, "TEST", &t);
    CHECK(r > 0 && (size_t)r == strlen(rep));
    CHECK(strstr(rep, "MODE:TEST\n") == rep);
    CHECK(strstr(rep, "GPS: fix=1 sats=08 lat=+40.389462 lon=-003.627416 alt=+00628.4m time=12:43:58\n") != NULL);
    CHECK(strstr(rep, "IMU: ax=-000.12m/s2 ay=+000.04m/s2 az=+009.81m/s2\n") != NULL);
    CHECK(strstr(rep, "IMU: gx=+0000.5deg/s gy=-0001.2deg/s gz=+0000.1deg/s\n") != NULL);
    CHECK(strstr(rep, "ATT: roll=+002.40deg pitch=-001.10deg yaw=+045.00deg\n") != NULL);
    CHECK(strstr(rep, "LIGHT: 073.5%\n") != NULL);
    CHECK(strstr(rep, "ENV: temp=+024.6C hum=045.2%\n") != NULL);
    CHECK(strstr(rep, "STATUS: FLYING\n") != NULL);
    CHECK(strstr(rep, "FLAGS: 0x0000\n") != NULL);

    /* 6. Informe sin GPS: hora desconocida */
    r = report_format(rep, sizeof rep, "NORMAL", &z);
    CHECK(r > 0 && strstr(rep, "time=--:--:--") != NULL && strstr(rep, "MODE:NORMAL\n") == rep);

    /* 7. Buffer insuficiente */
    CHECK(report_format(small, sizeof small, "TEST", &t) < 0);

    printf("%s (%d fallos)\n", failures ? "FALLO" : "OK", failures);
    return failures != 0;
}
