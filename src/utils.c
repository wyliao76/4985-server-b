#include "utils.h"
#include "starter.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__) && defined(__clang__)
_Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")
#endif

#define SIG_BUF 50
#define BASE_TEN 10

    int verbose                     = 0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
volatile sig_atomic_t running       = 1;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
volatile sig_atomic_t server_switch = 3;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

/* Calls `free()` and nullifies the ptr. */
void nfree(void **ptr)
{
    if(ptr != NULL && *ptr != NULL)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

static void handle_signal(int sig)
{
    char message[SIG_BUF];

    snprintf(message, sizeof(message), "Caught signal: %d (%s)\n", sig, strsignal(sig));
    write(STDOUT_FILENO, message, strlen(message));

    if(sig == SIGCHLD)
    {
        snprintf(message, sizeof(message), "\n%s\n", "Getting SIGCHLD...");
    }
    if(sig == SIGINT)
    {
        running = 0;
        snprintf(message, sizeof(message), "\n%s\n", "Shutting down gracefully...");
    }
    if(sig == SIGTSTP)
    {
        // switch between 0 and 1
        server_switch = (server_switch + 1) & 0x1;
    }
    write(STDOUT_FILENO, message, strlen(message));
}

void setup_signal(int handle_sigtstp)
{
    struct sigaction sa;

    sa.sa_handler = handle_signal;    // Set handler function for SIGINT
    sigemptyset(&sa.sa_mask);         // Don't block any additional signals
    sa.sa_flags = 0;

    // Register signal handler
    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }

    if(handle_sigtstp)
    {
        if(sigaction(SIGTSTP, &sa, NULL) == -1)
        {
            perror("sigaction SIGTSTP");
            exit(EXIT_FAILURE);
        }
    }
}

int convert(const char *str)
{
    char *endptr;
    long  sm_fd;

    errno = 0;
    if(!str)
    {
        return -1;
    }

    sm_fd = strtol(str, &endptr, BASE_TEN);

    // Check for conversion errors
    if((errno == ERANGE && (sm_fd == INT_MAX || sm_fd == INT_MIN)) || (errno != 0 && sm_fd > 2))
    {
        fprintf(stderr, "Error during conversion: %s\n", strerror(errno));
        return -1;
    }

    // Check if the entire string was converted
    if(endptr == str)
    {
        fprintf(stderr, "No digits were found in the input.\n");
        return -1;
    }

    // Check for leftover characters in the string
    if(*endptr != '\0')
    {
        fprintf(stderr, "Extra characters after the number: %s\n", endptr);
        return -1;
    }

    printf("sm_fd: %ld\n", sm_fd);
    return (int)sm_fd;
}
