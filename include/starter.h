#ifndef STARTER_H
#define STARTER_H

#include "fsm.h"

typedef enum
{
    PARSE_ENVP,
    CONNECT_SM,
    WAIT_FOR_START,
    LAUNCH_SERVER,
    CLEANUP_HANDLER,
} fsm_state_starter;

fsm_state_t parse_envp(void *args);

fsm_state_t connect_sm(void *args);

fsm_state_t wait_for_start(void *args);

fsm_state_t launch_server(void *args);

fsm_state_t cleanup_handler(void *args);

#endif    // STARTER_H
