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
    const char    start[SM_HEADER_SIZE] = {SVR_Start, VERSION, 0x00, 0x00};
    const char    stop[SM_HEADER_SIZE]  = {SVR_Stop, VERSION, 0x00, 0x00};
    char          buf[MSG_LEN];
    int           err;
    int           added;
    struct pollfd fds[2];

    setup_signal(1);

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

    for(int i = 1; i < 2; i++)
    {
        fds[i].fd = -1;
    }

    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;

    while(running)
    {
        if(poll(fds, 2, -1) < 0)
        {
            if(errno == EINTR)
            {
                errno = 0;

                if(server_switch == 0)
                {
                    printf("starting the server...\n");
                    write_fully(fds[1].fd, start, SM_HEADER_SIZE, &err);
                }
                else if(server_switch == 1)
                {
                    printf("stopping the server...\n");
                    write_fully(fds[1].fd, stop, SM_HEADER_SIZE, &err);
                }
                continue;
            }
            perror("Poll error");
            goto cleanup;
        }

        if(fds[0].revents & POLLIN)
        {
            client_fd = accept(server_fd, NULL, 0);
            printf("client_fd: %d\n", client_fd);
            if(client_fd < 0)
            {
                if(errno == EINTR)
                {
                    errno = 0;
                    goto done;
                }
                perror("Accept failed");
                continue;
            }
            // Add new client to poll list
            added = 0;
            for(int i = 1; i < 2; i++)
            {
                if(fds[i].fd == -1)
                {
                    fds[i].fd     = client_fd;
                    fds[i].events = POLLIN;
                    added         = 1;
                    break;
                }
            }
            if(!added)
            {
                char too_many[] = "Too many clients, rejecting connection\n";

                printf("%s", too_many);
                write_fully(client_fd, &too_many, (ssize_t)strlen(too_many), &err);

                close(client_fd);
                continue;
            }
        }

        if(fds[1].revents & POLLIN)
        {
            memset(buf, 0, MSG_LEN);
            if(read_fully(fds[1].fd, buf, SM_HEADER_SIZE, &err) < 0)
            {
                perror("read_fully error");
                continue;
            }
            if(buf[0] == SVR_Diagnostic)
            {
                if(read_fully(fds[1].fd, buf + SM_HEADER_SIZE, MSG_PAYLOAD_LEN, &err) < 0)
                {
                    perror("read_fully error");
                    continue;
                }
                // print
                for(int i = 0; i < MSG_LEN; i++)
                {
                    printf("%d ", buf[i]);
                }
                printf("\n");
                continue;
            }
            if(buf[0] == SVR_Online)
            {
                printf("server online\n");
            }
            if(buf[0] == SVR_Offline)
            {
                printf("server offline\n");
                close(fds[1].fd);
                fds[1].fd     = -1;
                fds[1].events = 0;
            }
            // print
            for(int i = 0; i < SM_HEADER_SIZE; i++)
            {
                printf("%d ", buf[i]);
            }
            printf("\n");
            continue;
        }
        if(fds[1].revents & (POLLHUP | POLLERR))
        {
            printf("server starter exit\n");
            return EXIT_SUCCESS;
        }
    }
done:
    return EXIT_SUCCESS;

cleanup:
    return EXIT_FAILURE;
}
