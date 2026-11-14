#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>

#include "gps_service.h"
#include "nmea.h"
#include "telemetry_data.h"
#include "config.h"

#define GPS_NODE            DT_ALIAS(gps)

#define LINE_MAX_LEN        96      /* una frase NMEA mide como mucho unos 82 caracteres */
#define LINE_QUEUE_LEN      8

#define GPS_STACK_SIZE      2048    /* PROVISIONAL: medir el uso real en la placa */
#define GPS_PRIORITY        6       /* PROVISIONAL: justificar frente a los hilos SIMU e IMU */
#define GPS_STALE_MS        (3 * GPS_PERIOD_MS)

struct line { char text[LINE_MAX_LEN]; };

K_MSGQ_DEFINE(line_q, sizeof(struct line), LINE_QUEUE_LEN, 4);

static const struct device *const gps_dev = DEVICE_DT_GET(GPS_NODE);

/* Estado que solo toca la interrupcion de la UART */
static struct line cur;
static size_t cur_len;
static bool discarding;

static atomic_t stat_gga_ok;
static atomic_t stat_bad;
static atomic_t stat_dropped;

void gps_service_get_stats(gps_stats_t *out)
{
    out->gga_ok = (uint32_t)atomic_get(&stat_gga_ok);
    out->lines_bad = (uint32_t)atomic_get(&stat_bad);
    out->lines_dropped = (uint32_t)atomic_get(&stat_dropped);
}

/* Interrupcion: solo monta lineas y las encola. Nada de parseo ni de mutex aqui. */
static void gps_uart_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    ARG_UNUSED(user_data);

    if (!uart_irq_update(dev)) {
        return;
    }

    while (uart_irq_rx_ready(dev)) {
        if (uart_fifo_read(dev, &c, 1) <= 0) {
            break;
        }

        if (c == '\n') {
            if (!discarding && cur_len > 0) {
                cur.text[cur_len] = '\0';
                if (k_msgq_put(&line_q, &cur, K_NO_WAIT) != 0) {
                    atomic_inc(&stat_dropped);
                }
            }
            cur_len = 0;
            discarding = false;
        } else if (c != '\r') {
            if (discarding) {
                continue;
            }
            if (cur_len < LINE_MAX_LEN - 1) {
                cur.text[cur_len++] = (char)c;
            } else {
                cur_len = 0;
                discarding = true;      /* linea demasiado larga: ignorar hasta el siguiente '\n' */
            }
        }
    }
}

static void gps_publish(const nmea_gga_t *g)
{
    /* Solo hay fix si la posicion es legible y la calidad es > 0 */
    uint8_t fix = g->position_valid ? 1 : 0;
    int32_t alt = (fix && g->alt_valid) ? g->alt_dm : 0;

    /* Sin fix, telemetry_set_gps conserva la ultima posicion y activa FLAG_GPS_INVALID */
    telemetry_set_gps(fix, g->sats, g->lat_e6, g->lon_e6, alt);

    if (fix && g->time_valid) {
        telemetry_set_gps_time(g->utc_hhmmss);   /* solo se fia de la hora si hay fix */
    }
}

static void gps_thread(void *p1, void *p2, void *p3)
{
    struct line l;
    nmea_gga_t gga;
    bool stale = false;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(gps_dev)) {
        telemetry_flag_set(FLAG_GPS_INVALID);
        return;
    }

    uart_irq_callback_user_data_set(gps_dev, gps_uart_cb, NULL);
    uart_irq_rx_enable(gps_dev);

    int64_t last_gga = k_uptime_get();

    while (1) {
        /* Bloquea hasta que llegue una linea; el timeout solo sirve para detectar silencio */
        if (k_msgq_get(&line_q, &l, K_MSEC(GPS_STALE_MS)) == 0) {
            int r = nmea_parse_gga(l.text, &gga);

            if (r == NMEA_OK) {
                atomic_inc(&stat_gga_ok);
                last_gga = k_uptime_get();
                stale = false;
                gps_publish(&gga);
            } else if (r != NMEA_ERR_NOT_GGA) {
                atomic_inc(&stat_bad);      /* checksum erroneo o GGA ilegible */
            }
            /* NMEA_ERR_NOT_GGA: RMC, GSA, GSV... se ignoran sin coste */
        }

        /* Sin GGA validas durante GPS_STALE_MS: el modulo ha dejado de responder */
        if (!stale && (k_uptime_get() - last_gga) > GPS_STALE_MS) {
            stale = true;
            telemetry_set_gps(0, 0, 0, 0, 0);
        }
    }
}

K_THREAD_DEFINE(gps_tid, GPS_STACK_SIZE, gps_thread, NULL, NULL, NULL, GPS_PRIORITY, 0, 0);