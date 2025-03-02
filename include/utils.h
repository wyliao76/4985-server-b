#ifndef UTILS_H
#define UTILS_H

#include "fsm.h"
#include <signal.h>

extern volatile sig_atomic_t running;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

void nfree(void **ptr);

fsm_state_t setup_signal(void *args);

#endif
