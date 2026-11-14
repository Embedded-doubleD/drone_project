#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include "fsm.h"
#include "events.h"
#include "led_service.h"
#include "output_service.h"
#include "telemetry_data.h"

#define FSM_STACK_SIZE   1024   /* PROVISIONAL: medir el uso real en la placa */
#define FSM_PRIORITY     5      /* PROVISIONAL */
#define DEBOUNCE_MS      0      /* ignora rebotes del pulsador (lo gestiona la placa en nuestro caso) */
#define LED_REFRESH_MS   500    /* en NORMAL, refresca el color con el estado del tracker */

/* El modo lo escribe solo este hilo y lo leen otros: atomic evita lecturas a medias */
static atomic_t mode = ATOMIC_INIT(FSM_MODE_TEST);   /* el sistema arranca en TEST */

fsm_mode_t fsm_get_mode(void)
{
    return (fsm_mode_t)atomic_get(&mode);
}

static led_color_t color_for_state(tracker_state_t st)
{
    switch (st) {                      /* NORMAL: verde IDLE, amarillo FLYING, rojo PROBLEM */
    case TRACKER_FLYING:  return LED_COLOR_YELLOW;
    case TRACKER_PROBLEM: return LED_COLOR_RED;
    case TRACKER_IDLE:
    default:              return LED_COLOR_GREEN;
    }
}

static led_color_t color_for(fsm_mode_t m)
{
    telemetry_t t;

    if (m == FSM_MODE_TEST) {
        return LED_COLOR_BLUE;         /* TEST = azul */
    }
    if (m == FSM_MODE_SIMU) {
        return LED_COLOR_MAGENTA;      /* SIMU = magenta, independiente del estado */
    }
    telemetry_get(&t);
    return color_for_state(t.state);
}

static fsm_mode_t next_mode(fsm_mode_t m)
{
    switch (m) {
    case FSM_MODE_TEST:   return FSM_MODE_NORMAL;
    case FSM_MODE_NORMAL: return FSM_MODE_SIMU;
    case FSM_MODE_SIMU:
    default:              return FSM_MODE_TEST;   /* ciclo TEST -> NORMAL -> SIMU -> TEST */
    }
}

static void enter_mode(fsm_mode_t m)
{
    atomic_set(&mode, m);
    output_service_set_mode(m);     /* cambia la cadencia y el tipo de salida */
    led_set_color(color_for(m));
}

static void fsm_thread(void *p1, void *p2, void *p3)
{
    event_t event;
    int64_t last_press = 0;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (led_init() != 0) {
        return;                      /* sin LED no se puede continuar; sin texto para no ensuciar la salida */
    }
    enter_mode(FSM_MODE_TEST);

    while (1) {
        int ret = events_receive(&event, K_MSEC(LED_REFRESH_MS));

        if (ret == 0 && event.type == EVENT_BUTTON_SHORT_PRESS) {
            int64_t now = k_uptime_get();
            if (now - last_press >= DEBOUNCE_MS) {
                last_press = now;
                enter_mode(next_mode(fsm_get_mode()));
            }
        } else if (fsm_get_mode() == FSM_MODE_NORMAL) {
            led_set_color(color_for(FSM_MODE_NORMAL));   /* sigue el estado IDLE/FLYING/PROBLEM */
        }
        /* Otros eventos (lecturas ADC, pulsacion larga) no cambian el modo */
    }
}

K_THREAD_DEFINE(fsm_tid, FSM_STACK_SIZE, fsm_thread, NULL, NULL, NULL, FSM_PRIORITY, 0, 0);