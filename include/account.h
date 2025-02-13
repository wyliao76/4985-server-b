#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "messaging.h"

ssize_t packet_handler(const request_t *request, response_t *response, int *err);

ssize_t account_create(const request_t *request, response_t *response, int *err);

ssize_t account_login(const request_t *request, response_t *response, int *err);

ssize_t account_logout(void);

#endif    // ACCOUNT_H
