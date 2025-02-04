#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>

int       tcp_socket(struct sockaddr_storage *sockaddr, int *err);
int       tcp_server(char *address, in_port_t port);
in_port_t convert_port(const char *str, int *err);

#endif
