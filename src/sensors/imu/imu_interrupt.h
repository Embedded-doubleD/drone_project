#ifndef IMU_INTERRUPT_H
#define IMU_INTERRUPT_H

#include <zephyr/kernel.h>

int imu_interrupt_init(void);
int imu_interrupt_enable(void);
int imu_interrupt_wait(k_timeout_t timeout);
void imu_interrupt_reset(void);

#endif /* IMU_INTERRUPT_H */
