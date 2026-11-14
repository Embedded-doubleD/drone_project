#ifndef FSM_H
#define FSM_H

/* Modos de operación */
typedef enum {
    FSM_MODE_TEST = 0,
    FSM_MODE_NORMAL,
    FSM_MODE_SIMU,
} fsm_mode_t;

/* Returns the current state machine mode. */
fsm_mode_t fsm_get_mode(void);

#endif /* FSM_H */