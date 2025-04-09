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
#define INADDRESS2 "0.0.0.0"
#define BACKLOG 5
#define PORT "9000"
#define PORT2 "8080"
#define SERVER_PORT "8000"
#define MAX_FDS_SM 3
#define NO_ACTIVE_LEN 7
#define SM_CLIENT_LEN 26

typedef enum
{
    CLIENT_GetIp,
    MAN_ReturnIp
} sm_client_t;

static ssize_t get_server_ip(int client_fd, char *ip_str)
{
    struct sockaddr_in addr;
    socklen_t          addr_len = sizeof(addr);

    if(getpeername(client_fd, (struct sockaddr *)&addr, &addr_len) == -1)
    {
        perror("getpeername");
        return -1;
    }
    if(inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN) == NULL)
    {
        perror("inet_ntop");
        return -1;
    }

    printf("Server_ip : %s\n", ip_str);
    return 0;
}

static void serialize_active_server(char *buf, const char *ip_str, const char *port)
{
    char   *ptr;
    uint8_t len;

    memset(buf, 0, SM_CLIENT_LEN);

    ptr    = buf;
    *ptr++ = MAN_ReturnIp;
    *ptr++ = VERSION;
    *ptr++ = 0x01;

    *ptr++ = UTF8STRING;

    len    = (uint8_t)strlen(ip_str);
    *ptr++ = (char)len;
    memcpy(ptr, ip_str, len);
    ptr += len;

    *ptr++ = UTF8STRING;
    len    = (uint8_t)strlen(port);
    *ptr++ = (char)len;
    memcpy(ptr, port, len);
}

int main(int argc, char *argv[])
{
    args_t        args;
    int           server_fd;
    int           server_fd2;
    int           client_fd;
    const char    start[SM_HEADER_SIZE]           = {SVR_Start, VERSION, 0x00, 0x00};
    const char    stop[SM_HEADER_SIZE]            = {SVR_Stop, VERSION, 0x00, 0x00};
    const char    error_msg[SM_HEADER_SIZE]       = {MAN_Error, VERSION, 0x00, 0x00};
    const char    no_active_server[NO_ACTIVE_LEN] = {MAN_Error, VERSION, 0x00, 0x0C, 0x00, 0x0C, 0x00};
    char          buf[SM_CLIENT_LEN];
    char          ip_str[INET_ADDRSTRLEN];
    int           err;
    int           added;
    struct pollfd fds[MAX_FDS_SM];
    int           active_server;

    setup_signal(1);

    printf("Server manager launching... (press Ctrl+C to interrupt)\n");

    memset(&args, 0, sizeof(args_t));
    args.addr = INADDRESS;
    convert_port(PORT, &args.port);
    args.sm_addr = INADDRESS2;
    convert_port(PORT2, &args.sm_port);
    verbose = convert(getenv("VERBOSE"));

    memset(buf, 0, SM_CLIENT_LEN);
    memset(ip_str, 0, INET_ADDRSTRLEN);
    active_server = 0;

    get_arguments(&args, argc, argv);

    printf("verbose: %d\n", verbose);
    PRINT_VERBOSE("%s\n", "verbose on");
    PRINT_DEBUG("%s\n", "debug on");

    err = 0;
    // Start TCP Server for starter
    server_fd = tcp_server(args.addr, args.port, BACKLOG, &err);
    if(server_fd < 0)
    {
        fprintf(stderr, "main::tcp_server: Failed to create TCP server. %d\n", err);
        return EXIT_FAILURE;
    }

    printf("Listening on %s:%d for stater\n", args.addr, args.port);

    err = 0;
    // Start TCP Server for clients
    server_fd2 = tcp_server(args.sm_addr, args.sm_port, BACKLOG, &err);
    if(server_fd2 < 0)
    {
        fprintf(stderr, "main::tcp_server: Failed to create TCP server. %d\n", err);
        return EXIT_FAILURE;
    }
    printf("Listening on %s:%d for clients\n", args.sm_addr, args.sm_port);

    for(int i = 2; i < MAX_FDS_SM; i++)
    {
        fds[i].fd = -1;
    }

    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;

    fds[1].fd     = server_fd2;
    fds[1].events = POLLIN;

    while(running)
    {
        if(poll(fds, MAX_FDS_SM, -1) < 0)
        {
            if(errno == EINTR)
            {
                errno = 0;

                if(server_switch == 0)
                {
                    PRINT_VERBOSE("%s\n", "starting the server...");
                    write_fully(fds[2].fd, start, SM_HEADER_SIZE, &err);
                }
                else if(server_switch == 1)
                {
                    PRINT_VERBOSE("%s\n", "stopping the server...");
                    write_fully(fds[2].fd, stop, SM_HEADER_SIZE, &err);
                }
                continue;
            }
            perror("Poll error");
            goto cleanup;
        }

        // to accept starter
        if(fds[0].revents & POLLIN)
        {
            client_fd = accept(server_fd, NULL, 0);
            printf("starter fd: %d\n", client_fd);
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
            for(int i = 2; i < MAX_FDS_SM; i++)
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
                PRINT_VERBOSE("%s\n", "Too many starters");
                write_fully(client_fd, &error_msg, (ssize_t)strlen(error_msg), &err);

                close(client_fd);
                continue;
            }
        }
        // accept clients
        if(fds[1].revents & POLLIN)
        {
            client_fd = accept(server_fd2, NULL, 0);
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

            if(active_server == 0)
            {
                PRINT_VERBOSE("%s\n", "sending no active server");
                if(verbose == 2)
                {
                    // print
                    for(int i = 0; i < NO_ACTIVE_LEN; i++)
                    {
                        printf("%d ", no_active_server[i]);
                    }
                    printf("\n");
                }

                write_fully(client_fd, &no_active_server, NO_ACTIVE_LEN, &err);
            }
            else
            {
                if(get_server_ip(fds[2].fd, ip_str) < 0)
                {
                    perror("get_server_ip failed");
                    continue;
                }

                serialize_active_server(buf, ip_str, SERVER_PORT);

                PRINT_VERBOSE("%s\n", "sending server address and port");
                write_fully(client_fd, &buf, (ssize_t)strlen(buf), &err);
            }

            close(client_fd);
            continue;
        }

        // starter
        if(fds[2].revents & POLLIN)
        {
            memset(buf, 0, MSG_LEN);
            if(read_fully(fds[2].fd, buf, SM_HEADER_SIZE, &err) < 0)
            {
                perror("read_fully error");
                continue;
            }
            if(buf[0] == SVR_Diagnostic)
            {
                if(read_fully(fds[2].fd, buf + SM_HEADER_SIZE, MSG_PAYLOAD_LEN, &err) < 0)
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
                active_server = 1;
            }
            if(buf[0] == SVR_Offline)
            {
                printf("server offline\n");
                close(fds[2].fd);
                fds[2].fd     = -1;
                fds[2].events = 0;
                active_server = 0;
            }
            if(verbose == 2)
            {
                // print
                for(int i = 0; i < SM_HEADER_SIZE; i++)
                {
                    printf("%d ", buf[i]);
                }
                printf("\n");
            }
            continue;
        }
        if(fds[2].revents & (POLLHUP | POLLERR))
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
