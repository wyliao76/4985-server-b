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

#endif    // STARTER_H
