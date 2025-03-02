// cppcheck-suppress-file unusedStructMember

#ifndef ARGS_H
#define ARGS_H

#include "fsm.h"
#include <arpa/inet.h>
#include <unistd.h>

typedef struct args_t
{
    const char *addr;
    in_port_t   port;
    const char *sm_addr;
    in_port_t   sm_port;
} args_t;

typedef struct fsm_args_t
{
    args_t *args;
    int     argc;
    char  **argv;
} fsm_args_t;

_Noreturn void usage(const char *binary_name, int exit_code, const char *message);

fsm_state_t get_arguments(void *args);

#endif    // ARGS_H
