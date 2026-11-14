#ifndef OUTPUT_SERVICE_H
#define OUTPUT_SERVICE_H

#include "fsm.h"

/*
Selecciona la salida segun el modo y reprograma el temporizador:
    TEST: informe legible cada TEST_PERIOD_MS
    NORMAL: informe legible cada NORMAL_PERIOD_MS
    SIMU: una trama SIMU cada SIMU_PERIOD_MS (unica salida permitida)
Se llama desde el hilo de la FSM al entrar en cada modo. */
void output_service_set_mode(fsm_mode_t mode);

#endif /* OUTPUT_SERVICE_H */