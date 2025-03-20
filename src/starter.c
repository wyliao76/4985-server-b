#include "args.h"
#include "networking.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTADDRESS "127.0.0.1"
#define SM_PORT "8080"

_Noreturn static void launch_server(pid_t *pid, char *argv[])
{
    int status;

    *pid = fork();
    if(*pid < 0)
    {
        perror("main::fork");
        exit(EXIT_FAILURE);
    }

    if(*pid == 0)
    {
        execv("./build/server", argv);

        // _exit if fails
        perror("execv failed");
        _exit(EXIT_FAILURE);
    }

    if(waitpid(*pid, &status, 0) == -1)
    {
        perror("Error waiting for child process");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    // int    err;
    args_t args;
    pid_t  pid;

    setup_signal();

    printf("Starter launching... (press Ctrl+C to interrupt)\n");

    // err = 0;

    memset(&args, 0, sizeof(args_t));
    // args.addr = INADDRESS;
    // convert_port(PORT, &args.port);
    args.sm_addr = OUTADDRESS;
    convert_port(SM_PORT, &args.sm_port);

    get_arguments(&args, argc, argv);

    launch_server(&pid, argv);
}
