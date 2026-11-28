#include <zephyr/kernel.h>
#include <string.h>

#include "processing/advanced_stats.h"

K_MUTEX_DEFINE(advanced_stats_mutex);

static advanced_stats_t stats;

void advanced_stats_init(void)
{
    k_mutex_lock(&advanced_stats_mutex, K_FOREVER);

    memset(&stats, 0, sizeof(stats));
    stats.latest_event = ADV_EVENT_NONE;

    k_mutex_unlock(&advanced_stats_mutex);
}

void advanced_stats_record_event(advanced_event_t event)
{
    k_mutex_lock(&advanced_stats_mutex, K_FOREVER);

    switch (event) {
    case ADV_EVENT_FREE_FALL:
        stats.free_fall_count++;
        break;

    case ADV_EVENT_MOTION:
        stats.motion_count++;
        break;

    case ADV_EVENT_IMPACT:
        stats.impact_count++;
        break;

    case ADV_EVENT_NONE:
    default:
        k_mutex_unlock(&advanced_stats_mutex);
        return;
    }

    stats.latest_event = event;
    stats.latest_timestamp_ms = k_uptime_get_32();

    k_mutex_unlock(&advanced_stats_mutex);
}

void advanced_stats_reset(void)
{
    k_mutex_lock(&advanced_stats_mutex, K_FOREVER);

    memset(&stats, 0, sizeof(stats));
    stats.latest_event = ADV_EVENT_NONE;

    k_mutex_unlock(&advanced_stats_mutex);
}

void advanced_stats_get(advanced_stats_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    k_mutex_lock(&advanced_stats_mutex, K_FOREVER);

    *snapshot = stats;

    k_mutex_unlock(&advanced_stats_mutex);
}