#include "args.h"
#include "io.h"
#include "messaging.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define TIMEOUT 3000    // 3s
#define INADDRESS "0.0.0.0"
#define OUTADDRESS "127.0.0.1"
// #define BACKLOG 5
#define PORT "8081"
#define SM_PORT "9000"
#define SM_HEADER_SIZE 4

// _Noreturn static void launch_server(char *argv[])
// {
//     pid_t pid;
//     int   status;

//     pid = fork();
//     if(pid < 0)
//     {
//         perror("main::fork");
//         exit(EXIT_FAILURE);
//     }

//     if(pid == 0)
//     {
//         // char *new_env[] = {"MY_VAR=HelloWorld", NULL};
//         // char *argv[] = {"./build/server", NULL};
//         // execv(argv[0], argv);
//         execv("./build/server", argv);

//         // _exit if fails
//         perror("execv failed");
//         _exit(EXIT_FAILURE);
//     }

//     if(waitpid(pid, &status, 0) == -1)
//     {
//         perror("Error waiting for child process");
//         exit(EXIT_FAILURE);
//     }
//     exit(EXIT_SUCCESS);
// }

static void connect_sm(args_t *args)
{
    while(running)
    {
        // Start TCP Client
        printf("Connecting to server manager...\n");
        *args->sm_fd = tcp_client(args->sm_addr, args->sm_port, args->err);
        printf("sm_fd: %d\n", *args->sm_fd);
        if(*args->sm_fd > 0)
        {
            printf("Connect to server manager at %s:%d\n", args->sm_addr, args->sm_port);
            break;
        }
        perror("Failed to create TCP client.");
        errno      = 0;
        *args->err = 0;
        sleep(3);
    }
}

int main(int argc, char *argv[])
{
    int           err;
    args_t        args;
    int           status;
    char          buf[SM_HEADER_SIZE];
    pid_t         pid;
    int           sm_fd;
    struct pollfd fds[1];
    ssize_t       result;

    setup_signal();

    printf("Starter launching... (press Ctrl+C to interrupt)\n");

    err = 0;
    memset(&args, 0, sizeof(args_t));
    args.addr = INADDRESS;
    convert_port(PORT, &args.port);
    args.sm_addr = OUTADDRESS;
    convert_port(SM_PORT, &args.sm_port);
    args.sm_fd = &sm_fd;
    args.err   = &err;

    get_arguments(&args, argc, argv);

    connect_sm(&args);

    fds[0].fd     = *args.sm_fd;
    fds[0].events = POLLIN;

    while(running)
    {
        printf("polling\n");
        result = poll(fds, 1, -1);
        if(result == -1)
        {
            perror("Poll error");
            // goto cleanup;
            exit(EXIT_FAILURE);
        }

        if(fds[0].revents & POLLIN)
        {
            printf("reading from sm\n");
            memset(buf, 0, SM_HEADER_SIZE);
            if(read_fully(*args.sm_fd, buf, SM_HEADER_SIZE, &err) < 0)
            {
                perror("read_fully error");
                errno = 0;
                continue;
            }
            for(int i = 0; i < 4; i++)
            {
                printf("%d ", buf[i]);
            }
            printf("\n");
            if(buf[0] == SVR_Start)
            {
                printf("Server start requested %d\n", buf[0]);
                break;
            }
        }
        if(fds[0].revents & (POLLHUP | POLLERR))
        {
            printf("sever manager exit\n");
            exit(EXIT_SUCCESS);
        }
    }

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

    while(running)
    {
        errno = 0;
        printf("polling...\n");
        result = poll(fds, 1, TIMEOUT);
        if(result == -1)
        {
            perror("Poll error");
            // goto cleanup;
            break;
        }
        if(result == 0)
        {
            printf("check child\n");
            if(waitpid(pid, &status, WNOHANG) > 0)
            {
                printf("Child exited, status = %d\n", WEXITSTATUS(status));
                break;
            }
        }
        if(fds[0].revents & POLLIN)
        {
            memset(buf, 0, SM_HEADER_SIZE);
            if(read_fully(*args.sm_fd, buf, SM_HEADER_SIZE, &err) < 0)
            {
                perror("read_fully error");
                errno = 0;
                continue;
            }
            printf("Received byte: %d\n", buf[0]);
            if(buf[0] == SVR_Stop)
            {
                printf("Killing child process...\n");
                kill(pid, SIGINT);
                waitpid(pid, &status, 0);    // Ensure child cleanup
                break;
            }
        }
        if(fds[0].revents & (POLLHUP | POLLERR))
        {
            printf("server manager exit\n");
            printf("Killing child process...\n");
            kill(pid, SIGINT);
            waitpid(pid, &status, 0);    // Ensure child cleanup
            break;
        }
    }

    kill(pid, SIGINT);
    waitpid(pid, &status, 0);    // Ensure child cleanup

    // launch_server(argv);

    close(*args.sm_fd);
}
