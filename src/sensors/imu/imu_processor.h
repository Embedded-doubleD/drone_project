#ifndef IMU_PROCESSOR_H
#define IMU_PROCESSOR_H

#include <stdint.h>

#include "imu_types.h"

void imu_processor_init(void);
void imu_processor_process(const imu_data_t *imu,
                           const imu_data_t *corrected_imu,
                           int64_t now_ms);

#endif /* IMU_PROCESSOR_H */
