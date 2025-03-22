// cppcheck-suppress-file unusedStructMember

#ifndef ARGS_H
#define ARGS_H

#include <arpa/inet.h>
#include <unistd.h>
#define ARGC 10

typedef struct args_t
{
    const char *addr;
    in_port_t   port;
    const char *sm_addr;
    in_port_t   sm_port;
    int         backlog;
    int        *sm_fd;
    int        *err;
    char       *envp[ARGC];
} args_t;

_Noreturn void usage(const char *binary_name, int exit_code, const char *message);

void get_arguments(args_t *args, int argc, char *argv[]);

#endif    // ARGS_H
