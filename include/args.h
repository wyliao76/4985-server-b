#ifndef ARGS_H
#define ARGS_H

#include <arpa/inet.h>

typedef struct Arguments
{
    // cppcheck-suppress unusedStructMember
    const char *addr;
    // cppcheck-suppress unusedStructMember
    in_port_t port;
    // cppcheck-suppress unusedStructMember
    const char *sm_addr;
    // cppcheck-suppress unusedStructMember
    in_port_t sm_port;
} Arguments;

typedef struct HOST
{
    // cppcheck-suppress unusedStructMember
    const char *addr;
    // cppcheck-suppress unusedStructMember
    in_port_t port;
} HOST;

_Noreturn void usage(const char *binary_name, int exit_code, const char *message);

void get_arguments(Arguments *args, int argc, char *argv[]);

void validate_arguments(const char *binary_name, const Arguments *args);

#endif    // ARGS_H
