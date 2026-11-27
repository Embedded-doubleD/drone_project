#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "output_service.h"
#include "simu_frame.h"
#include "telemetry_report.h"
#include "telemetry_data.h"
#include "config.h"
#include "processing/statistics.h"
#include "app/config.h"
// Temporal
#include "gps_service.h"

#define OUT_STACK_SIZE    2048    /* PROVISIONAL: medir el uso real (buffers de 222 y 640 bytes) */
#define OUT_PRIORITY      4       /* PROVISIONAL: más prioritario que los sensores */
#define STATISTICS_PERIOD_MS 30000

/* Consola = Virtual COM del ST-LINK, el canal que lee el simulador */
static const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
extern const k_tid_t imu_thread_id;

static K_SEM_DEFINE(tick_sem, 0, 1);
static K_SEM_DEFINE(statistics_tick_sem, 0, 1);

static void tick_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    k_sem_give(&tick_sem);
}

static void statistics_tick_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    k_sem_give(&statistics_tick_sem);
}

K_TIMER_DEFINE(tick_timer, tick_handler, NULL);
K_TIMER_DEFINE(statistics_timer, statistics_tick_handler, NULL);

void output_service_set_mode(fsm_mode_t mode) {
    uint32_t period;

    switch (mode) {
        case FSM_MODE_SIMU: period = SIMU_PERIOD_MS; break;
        case FSM_MODE_NORMAL: period = NORMAL_PERIOD_MS; break;
        case FSM_MODE_TEST:
        default: period = TEST_PERIOD_MS; break;
    }

    k_timer_start(&tick_timer, K_MSEC(period), K_MSEC(period));
    k_sem_give(&tick_sem);    /* primera salida inmediata al entrar en el modo */

    k_timer_stop(&statistics_timer);
    k_sem_reset(&statistics_tick_sem);

    if (mode == FSM_MODE_NORMAL) {
        statistics_reset();
        k_timer_start(&statistics_timer,
                      K_MSEC(STATISTICS_PERIOD_MS),
                      K_MSEC(STATISTICS_PERIOD_MS));
    }
}

/* Se escribe byte a byte con uart_poll_out y NO con printk: printk convierte '\n'
en "\r\n" y la trama ya lleva su propio "\r\n", lo que daría 222 caracteres. */
static void write_raw(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(console_dev, (unsigned char)buf[i]);
    }
}

static void print_statistics(void)
{
    statistics_snapshot_t stats;
    size_t imu_stack_unused;

    statistics_take_snapshot(&stats);

    printk("\nSTATISTICS (30 s):\n");

    if (k_thread_stack_space_get(imu_thread_id, &imu_stack_unused) == 0) {
        printk("IMU STACK: unused=%u bytes\n",
               (unsigned)imu_stack_unused);
    }

    if (stats.light.count > 0) {
        printk("LIGHT: mean=%.1f %% min=%.1f %% max=%.1f %%\n",
               (double)stats.light.mean,
               (double)stats.light.min,
               (double)stats.light.max);
    } else {
        printk("LIGHT: no valid samples\n");
    }

    if (stats.temperature.count > 0) {
        printk("TEMP: mean=%.1f C min=%.1f C max=%.1f C\n",
               (double)stats.temperature.mean,
               (double)stats.temperature.min,
               (double)stats.temperature.max);
    } else {
        printk("TEMP: no valid samples\n");
    }

    if (stats.humidity.count > 0) {
        printk("HUM: mean=%.1f %% min=%.1f %% max=%.1f %%\n",
               (double)stats.humidity.mean,
               (double)stats.humidity.min,
               (double)stats.humidity.max);
    } else {
        printk("HUM: no valid samples\n");
    }

    if (stats.acceleration.count > 0) {
        printk("ACC: mean=%.2f m/s2 min=%.2f m/s2 max=%.2f m/s2\n",
               (double)stats.acceleration.mean,
               (double)stats.acceleration.min,
               (double)stats.acceleration.max);
    } else {
        printk("ACC: no valid samples\n");
    }

    if (stats.angular_velocity.count > 0) {
        printk("GYRO: mean=%.2f deg/s min=%.2f deg/s max=%.2f deg/s\n",
               (double)stats.angular_velocity.mean,
               (double)stats.angular_velocity.min,
               (double)stats.angular_velocity.max);
    } else {
        printk("GYRO: no valid samples\n");
    }

    printk("\n");
}

static void out_thread(void *p1, void *p2, void *p3) {
    static char frame[SIMU_FRAME_BUF];
    static char report[REPORT_BUF];
    uint16_t seq = 0;
    telemetry_t t;

    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    if (!device_is_ready(console_dev)) {
        return;
    }

    while (1) {
        k_sem_take(&tick_sem, K_FOREVER);    /* duerme hasta el siguiente instante de salida */

        fsm_mode_t mode = fsm_get_mode();
        telemetry_get(&t);

        if (mode == FSM_MODE_NORMAL &&
            k_sem_take(&statistics_tick_sem, K_NO_WAIT) == 0) {
            print_statistics();
        }

        if (mode == FSM_MODE_SIMU) {
            int n = simu_frame_build(frame, sizeof frame, seq, k_uptime_get_32(), &t);
            if (n == SIMU_FRAME_LEN) {
                write_raw(frame, (size_t)n);
            }
            seq++;    /* uint16_t: tras 65535 vuelve a 00000 */
        } else {
            const char *name = (mode == FSM_MODE_TEST) ? "TEST" : "NORMAL";
            int n = report_format(report, sizeof report, name, &t);
            if (n > 0) {
                gps_stats_t gs;
                gps_service_get_stats(&gs);
                printk("GPS stats: ok=%u bad=%u dropped=%u\n",
                    (unsigned)gs.gga_ok, (unsigned)gs.lines_bad, (unsigned)gs.lines_dropped);
                printk("%s", report);
            }
        }
    }
}

K_THREAD_DEFINE(out_tid, OUT_STACK_SIZE, out_thread, NULL, NULL, NULL, OUT_PRIORITY, 0, 0);
