#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define LINE_MAX_LEN 96     /* una frase NMEA mide como mucho unos 82 caracteres */

struct line { char text[LINE_MAX_LEN]; };

K_MSGQ_DEFINE(line_q, sizeof(struct line), 8, 4);

static const struct device *const gps_dev = DEVICE_DT_GET(DT_ALIAS(gps));
static struct line cur;
static size_t cur_len;

static void uart_cb(const struct device *dev, void *user_data) {
    uint8_t c;

    ARG_UNUSED(user_data);
    uart_irq_update(dev);

    while (uart_irq_rx_ready(dev)) {
        if (uart_fifo_read(dev, &c, 1) <= 0) {
            break;
        }
        if (c == '\n') {
            if (cur_len > 0) {
                cur.text[cur_len] = '\0';
                k_msgq_put(&line_q, &cur, K_NO_WAIT);   /* si está llena, se pierde la línea */
            }
            cur_len = 0;
        } else if (c != '\r') {
            if (cur_len < LINE_MAX_LEN -1) {
                cur.text[cur_len++] = (char)c;
            } else {
                cur_len = 0;                            /* línea demasiado larga, se descarta */
            }
        }
    }
}

int main(void) {
    struct line l;

    if (!device_is_ready(gps_dev)) {
        printk("GPS UART not ready\n");
        return 0;
    }

    uart_irq_callback_user_data_set(gps_dev, uart_cb, NULL);
    uart_irq_rx_enable(gps_dev);
    printk("GPS test: waiting for NMEA...\n");

    while (1) {
        k_msgq_get(&line_q, &l, K_FOREVER);
        printk("%s\n", l.text);
    }
    return 0;
}