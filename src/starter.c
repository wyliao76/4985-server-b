#include "args.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

// #define TIMEOUT 3000    // 3s
#define INADDRESS "0.0.0.0"
#define OUTADDRESS "127.0.0.1"
// #define BACKLOG 5
#define PORT "8081"
#define SM_PORT "8080"

_Noreturn static void launch_server(char *argv[])
{
    pid_t pid;
    int   status;

    pid = fork();
    if(pid < 0)
    {
        perror("main::fork");
        exit(EXIT_FAILURE);
    }

    if(pid == 0)
    {
        // char *new_env[] = {"MY_VAR=HelloWorld", NULL};
        // char *argv[] = {"./build/server", NULL};
        // execv(argv[0], argv);
        execv("./build/server", argv);

        // _exit if fails
        perror("execv failed");
        _exit(EXIT_FAILURE);
    }

    if(waitpid(pid, &status, 0) == -1)
    {
        perror("Error waiting for child process");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int    err;
    args_t args;
    // struct pollfd fds[1];
    // ssize_t result;

    setup_signal();

    printf("Starter launching... (press Ctrl+C to interrupt)\n");

    err = 0;

    memset(&args, 0, sizeof(args_t));
    args.addr = INADDRESS;
    convert_port(PORT, &args.port);
    args.sm_addr = OUTADDRESS;
    convert_port(SM_PORT, &args.sm_port);

    get_arguments(&args, argc, argv);

    while(running)
    {
        int sm_fd;

        // Start TCP Client
        sm_fd = tcp_client(args.sm_addr, args.sm_port, &err);
        printf("sm_fd: %d\n", sm_fd);
        if(sm_fd < 0)
        {
            fprintf(stderr, "main::tcp_server: Failed to create TCP server.\n");
            errno = 0;
            sleep(3);
        }
        printf("Connect to server manager at %s:%d\n", args.sm_addr, args.sm_port);
    }

    // fds[0].fd     = sm_fd;
    // fds[0].events = POLLIN;

    launch_server(argv);

    // close(sm_fd);
}
