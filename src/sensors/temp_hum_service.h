#ifndef TEMP_HUM_SERVICE_H
#define TEMP_HUM_SERVICE_H

#include <stdint.h>

int temp_hum_init(void);
int temp_hum_read(int32_t *temp_dc, int32_t *hum_dpct);

#endif /* TEMP_HUM_SERVICE_H */
