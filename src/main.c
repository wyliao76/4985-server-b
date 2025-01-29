#include "networking.h"
#include "threads.h"
#include <arpa/inet.h>
#include <getopt.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define UNKNOWN_OPTION_MESSAGE_LEN 22

typedef struct
{
    char     *addr;
    in_port_t port;
} Arguments;

_Noreturn void usage(const char *binary_name, int exit_code, const char *message);
void           get_arguments(Arguments *args, int argc, char *argv[]);
void           validate_arguments(const char *binary_name, const Arguments *args);

int main(int argc, char *argv[])
{
    int sockfd;

    Arguments args;

    memset(&args, 0, sizeof(Arguments));
    get_arguments(&args, argc, argv);
    validate_arguments(argv[0], &args);

    printf("Listening on %s:%d\n", args.addr, args.port);

    // Start TCP Server
    sockfd = tcp_socket(args.addr, args.port);
    if(sockfd < 0)
    {
        fprintf(stderr, "main::tcp_socket: Failed to create TCP socket.\n");
        return EXIT_FAILURE;
    }

    // Wait for client connections
    while(1)
    {
        thread_args targs;

        int                connfd;
        struct sockaddr_in connaddr;
        socklen_t          connsize;

        // !!BLOCKING!! Get client connection
        connsize = sizeof(struct sockaddr_in);
        memset(&connaddr, 0, connsize);

        connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connsize);
        if(connfd < 0)
        {
            fprintf(stderr, "main::accept: Failed to accept client connection.\n");
            continue;
        }

        printf("New connection from: %s:%d\n", inet_ntoa(connaddr.sin_addr), connaddr.sin_port);

        // Start new thread to handle request
        memset(&targs, 0, sizeof(thread_args));
        targs.connfd = connfd;

        if(start_thread(thread_echo, &targs, 0) == -1)    // NOTE: Thread must close file descriptor!!
        {
            fprintf(stderr, "main::start_thread: Thread function is NULL.\n");
        }
    }

    return EXIT_SUCCESS;
}

_Noreturn void usage(const char *binary_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] -a <address> -p <port>\n", binary_name);
    fputs("Options:\n", stderr);
    fputs("  -h, --help                         Display this help message\n", stderr);
    fputs("  -a <address>, --address <address>  The address of remote server.\n", stderr);
    fputs("  -p <port>,    --port <port>        The server port to use.\n", stderr);
    exit(exit_code);
}

void get_arguments(Arguments *args, int argc, char *argv[])
{
    int opt;
    int err;

    static struct option long_options[] = {
        {"address", required_argument, NULL, 'a'},
        {"port",    required_argument, NULL, 'p'},
        {"help",    no_argument,       NULL, 'h'},
        {NULL,      0,                 NULL, 0  }
    };

    while((opt = getopt_long(argc, argv, "ha:p:", long_options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'a':
                args->addr = optarg;
                break;
            case 'p':
                args->port = convert_port(optarg, &err);

                if(err != 0)
                {
                    usage(argv[0], EXIT_FAILURE, "Port must be between 1 and 65535");
                }
                break;
            case 'h':
                usage(argv[0], EXIT_SUCCESS, NULL);
            case '?':
                if(optopt != 'a' && optopt != 'p')
                {
                    char message[UNKNOWN_OPTION_MESSAGE_LEN];

                    snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                    usage(argv[0], EXIT_FAILURE, message);
                }
                break;
            default:
                usage(argv[0], EXIT_FAILURE, NULL);
        }
    }
}

void validate_arguments(const char *binary_name, const Arguments *args)
{
    if(args->addr == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an ipv4 address to connect to.");
    }

    if(args->port == 0)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an available port to connect to.");
    }
}
