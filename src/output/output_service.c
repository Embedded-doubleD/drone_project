#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "output_service.h"
#include "simu_frame.h"
#include "telemetry_report.h"
#include "telemetry_data.h"
#include "config.h"
// Temporal
#include "gps_service.h"

#define OUT_STACK_SIZE    2048    /* PROVISIONAL: medir el uso real (buffers de 222 y 640 bytes) */
#define OUT_PRIORITY      4       /* PROVISIONAL: más prioritario que los sensores */

/* Consola = Virtual COM del ST-LINK, el canal que lee el simulador */
static const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static K_SEM_DEFINE(tick_sem, 0, 1);

static void tick_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    k_sem_give(&tick_sem);
}

K_TIMER_DEFINE(tick_timer, tick_handler, NULL);

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
}

/* Se escribe byte a byte con uart_poll_out y NO con printk: printk convierte '\n'
en "\r\n" y la trama ya lleva su propio "\r\n", lo que daría 222 caracteres. */
static void write_raw(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(console_dev, (unsigned char)buf[i]);
    }
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
