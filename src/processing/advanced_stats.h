#ifndef ADVANCED_STATS_H
#define ADVANCED_STATS_H

#include <stdint.h>

/* Events detected by the MPU6050 processing module. */
typedef enum {
    ADV_EVENT_NONE = 0,
    ADV_EVENT_FREE_FALL,
    ADV_EVENT_MOTION,
    ADV_EVENT_IMPACT
} advanced_event_t;

/* Snapshot of the ADVANCED mode event counters. */
typedef struct {
    uint32_t free_fall_count;
    uint32_t motion_count;
    uint32_t impact_count;

    advanced_event_t latest_event;
    uint32_t latest_timestamp_ms;
} advanced_stats_t;

/* Initialize all counters and event information. */
void advanced_stats_init(void);

/* Register one detected event. */
void advanced_stats_record_event(advanced_event_t event);

/* Reset all counters and latest-event information. */
void advanced_stats_reset(void);

/* Obtain a consistent snapshot of the counters. */
void advanced_stats_get(advanced_stats_t *stats);

#endif /* ADVANCED_STATS_H */