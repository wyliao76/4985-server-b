#include "args.h"
#include "fsm.h"
#include "io.h"
#include "messaging.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INADDRESS "0.0.0.0"
#define BACKLOG 5
#define PORT "9000"

int main(void)
{
    args_t        args;
    int           server_fd;
    int           client_fd;
    char          start[4] = {SVR_Start, VERSION, 0x00, 0x00};
    char          stop[4]  = {SVR_Stop, VERSION, 0x00, 0x00};
    int           err;
    struct pollfd fds[2];

    setup_signal();

    printf("Server manager launching... (press Ctrl+C to interrupt)\n");

    memset(&args, 0, sizeof(args_t));
    args.addr = INADDRESS;
    convert_port(PORT, &args.port);

    err = 0;
    // Start TCP Server
    server_fd = tcp_server(args.addr, args.port, BACKLOG, &err);
    if(server_fd < 0)
    {
        fprintf(stderr, "main::tcp_server: Failed to create TCP server. %d\n", err);
        return EXIT_FAILURE;
    }

    printf("Listening on %s:%d\n", args.addr, args.port);

    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;
    client_fd     = 0;

    while(running)
    {
        if(poll(fds, 2, -1) < 0)
        {
            perror("Poll error");
            exit(EXIT_FAILURE);
        }
        if(fds[0].revents & POLLIN)
        {
            client_fd = accept(server_fd, NULL, 0);
            printf("client_fd: %d\n", client_fd);
            if(client_fd < 0)
            {
                if(errno == EINTR)
                {
                    goto cleanup;
                }
                perror("Accept failed");
                continue;
            }
            fds[1].fd     = client_fd;
            fds[1].events = POLLIN;

            write_fully(client_fd, start, 4, &err);
            printf("starting the server...\n");
        }
        if(fds[1].revents & (POLLHUP | POLLERR))
        {
            printf("server starter exit\n");
            return EXIT_SUCCESS;
        }
    }
    write_fully(client_fd, stop, 4, &err);

    return EXIT_SUCCESS;

cleanup:
    return EXIT_FAILURE;
}
