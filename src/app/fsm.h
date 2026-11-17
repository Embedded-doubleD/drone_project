#ifndef FSM_H
#define FSM_H

typedef enum {
    FSM_MODE_TEST = 0,
    FSM_MODE_NORMAL,
    FSM_MODE_SIMU,
} fsm_mode_t;

int fsm_init(void);
void fsm_process(void);
fsm_mode_t fsm_get_mode(void);

#endif /* FSM_H */