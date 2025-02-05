#include "args.h"
#include "messaging.h"
#include "networking.h"
#include <errno.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 1024
#define INADDRESS "0.0.0.0"
#define PORT "8081"

int main(int argc, char *argv[])
{
    pid_t     pid;
    int       sockfd;
    Arguments args;
    int       err;

    memset(&args, 0, sizeof(Arguments));
    args.addr = INADDRESS;
    args.port = convert_port(PORT, &err);

    get_arguments(&args, argc, argv);
    validate_arguments(argv[0], &args);

    printf("Listening on %s:%d\n", args.addr, args.port);

    // Start TCP Server
    sockfd = tcp_server(&args);
    if(sockfd < 0)
    {
        fprintf(stderr, "main::tcp_server: Failed to create TCP server.\n");
        return EXIT_FAILURE;
    }

    // Wait for client connections
    pid = 1;
    while(pid > 0)
    {
        int                connfd;
        struct sockaddr_in connaddr;
        socklen_t          connsize;

        // !!BLOCKING!! Get client connection
        connsize = sizeof(struct sockaddr_in);
        memset(&connaddr, 0, connsize);

        errno  = 0;
        connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connsize);
        if(connfd < 0)
        {
            perror("main::accept");
            continue;
        }

        printf("New connection from: %s:%d\n", inet_ntoa(connaddr.sin_addr), connaddr.sin_port);

        // Fork the process
        errno = 0;
        pid   = fork();
        if(pid < 0)
        {
            perror("main::fork");
            continue;
        }

        if(pid == 0)
        {
            close(sockfd);

            err = 0;
            if(copy(connfd, connfd, BUFLEN, &err) < 0)
            {
                errno = err;
                perror("main::copy");
            }
        }

        close(connfd);
    }

    return EXIT_SUCCESS;
}
