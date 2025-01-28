#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>

in_port_t convert_port(const char *str, int *err);

#endif
