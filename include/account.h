#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "messaging.h"

extern const funcMapping acc_func[];

ssize_t account_create(const request_t *request);

ssize_t account_login(const request_t *request);

ssize_t account_logout(const request_t *request);

#endif    // ACCOUNT_H
