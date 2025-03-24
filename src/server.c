#include "args.h"
#include "fsm.h"
#include "messaging.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <limits.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define INADDRESS "0.0.0.0"
#define OUTADDRESS "127.0.0.1"
#define BACKLOG 5
#define PORT "8081"
#define SM_PORT "8082"
#define BASE_TEN 10

static int convert(const char *str)
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

int main(int argc, char *argv[])
{
    int retval;
    int server_fd;
    // long        sm_fd_long;
    int         sm_fd;
    args_t      args;
    int         err;
    const char *addr    = getenv("ADDR");
    const char *port    = getenv("PORT");
    const char *sm_addr = getenv("SM_ADDR");
    const char *sm_port = getenv("SM_PORT");
    char      **env     = environ;

    while(*env)
    {
        printf("%s\n", *env);
        env++;
    }

    setup_signal();

    printf("Server launching... (press Ctrl+C to interrupt)\n");

    retval = EXIT_SUCCESS;
    err    = 0;

    memset(&args, 0, sizeof(args_t));
    args.addr = addr ? addr : INADDRESS;
    convert_port(port ? port : PORT, &args.port);
    args.sm_addr = sm_addr ? sm_addr : OUTADDRESS;
    convert_port(sm_port ? sm_port : SM_PORT, &args.sm_port);
    sm_fd = convert(getenv("SM_FD"));

    get_arguments(&args, argc, argv);

    // Start TCP Server
    server_fd = tcp_server(args.addr, args.port, BACKLOG, &err);
    if(server_fd < 0)
    {
        fprintf(stderr, "main::tcp_server: Failed to create TCP server. %d\n", err);
        return EXIT_FAILURE;
    }

    printf("Listening on %s:%d\n", args.addr, args.port);

    // Start TCP Client
    // sm_fd = tcp_client(args.sm_addr, args.sm_port, &err);
    // if(sm_fd < 0)
    // {
    //     fprintf(stderr, "main::tcp_server: Failed to create TCP server.\n");
    //     return EXIT_FAILURE;
    // }
    // printf("Connect to server manager at %s:%d\n", args.sm_addr, args.sm_port);

    printf("sm_fd: %d\n", sm_fd);

    // Wait for client connections
    errno = 0;
    err   = 0;
    // sm_fd = 0;
    event_loop(server_fd, sm_fd, &err);

    close(sm_fd);
    close(server_fd);
    return retval;
}
