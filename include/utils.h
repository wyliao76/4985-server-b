#ifndef UTILS_H
#define UTILS_H

#include <signal.h>

// clang-format off
#define PRINT_VERBOSE(fmt, ...) do { if (verbose >= 1) printf(fmt, __VA_ARGS__); } while (0)
#define PRINT_DEBUG(fmt, ...) do { if (verbose >= 2) printf(fmt, __VA_ARGS__); } while (0)
// clang-format on

// 1 = Verbose, 2 = Debug
extern int                   verbose;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern volatile sig_atomic_t running;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern volatile sig_atomic_t server_switch;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

void nfree(void **ptr);

void setup_signal(int handle_sigtstp);
int  convert(const char *str);

#endif
