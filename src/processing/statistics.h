#ifndef STATISTICS_H
#define STATISTICS_H

typedef struct {
    float mean;
    float min; 
    float max;
    unsigned int count;
} statistics_value_t;

typedef struct {
    statistics_value_t temperature;
    statistics_value_t humidity;
    statistics_value_t light;
    statistics_value_t acceleration;
    statistics_value_t angular_velocity;
} statistics_snapshot_t;

void statistics_reset(void);
void statistics_add_light(float value);
void statistics_add_environment(float temperature,
                                 float humidity);
void statistics_add_imu(float ax, float ay, float az,
                        float gx, float gy, float gz);
void statistics_take_snapshot(statistics_snapshot_t *snapshot);

#endif /* STATISTICS_H */