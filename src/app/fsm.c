#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "fsm.h"
#include "events.h"
#include "led_service.h"
#include "output_service.h"
#include "telemetry_data.h"
#include "config.h"

#define DEBOUNCE_MS      0

static atomic_t mode = ATOMIC_INIT(FSM_MODE_TEST);

static int64_t last_press;
static int64_t last_led_refresh;
static bool initialized;

fsm_mode_t fsm_get_mode(void)
{
    return (fsm_mode_t)atomic_get(&mode);
}

static led_color_t color_for_state(tracker_state_t st)
{
    switch (st) {
    case TRACKER_FLYING:
        return LED_COLOR_YELLOW;

    case TRACKER_PROBLEM:
        return LED_COLOR_RED;

    case TRACKER_IDLE:
    default:
        return LED_COLOR_GREEN;
    }
}

static led_color_t color_for(fsm_mode_t m)
{
    telemetry_t t;

    if (m == FSM_MODE_TEST) {
        return LED_COLOR_BLUE;
    }

    if (m == FSM_MODE_SIMU) {
        return LED_COLOR_MAGENTA;
    }

    telemetry_get(&t);
    return color_for_state(t.state);
}

static fsm_mode_t next_mode(fsm_mode_t m)
{
    switch (m) {
    case FSM_MODE_TEST:
        return FSM_MODE_NORMAL;

    case FSM_MODE_NORMAL:
        return FSM_MODE_SIMU;

    case FSM_MODE_SIMU:
    default:
        return FSM_MODE_TEST;
    }
}

static void enter_mode(fsm_mode_t m)
{
    atomic_set(&mode, m);

    output_service_set_mode(m);
    led_set_color(color_for(m));
}

int fsm_init(void)
{
    int ret = led_init();

    if (ret != 0) {
        return ret;
    }

    last_press = 0;
    last_led_refresh = k_uptime_get();
    initialized = true;

    enter_mode(FSM_MODE_TEST);

    return 0;
}

void fsm_process(void)
{
    event_t event;
    int64_t now;

    if (!initialized) {
        return;
    }

    /* Process all currently pending events without blocking. */
    while (events_receive(&event, K_NO_WAIT) == 0) {

        if (event.type == EVENT_BUTTON_SHORT_PRESS) {
            now = k_uptime_get();

            if (now - last_press >= DEBOUNCE_MS) {
                last_press = now;
                enter_mode(next_mode(fsm_get_mode()));
            }
        }
    }

    /* Refresh the LED periodically in NORMAL mode. */
    now = k_uptime_get();

    if ((fsm_get_mode() == FSM_MODE_NORMAL) &&
        (now - last_led_refresh >= LED_REFRESH_MS)) {

        led_set_color(color_for(FSM_MODE_NORMAL));
        last_led_refresh = now;
    }
}