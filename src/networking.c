#include "networking.h"
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static void setup_addr(struct sockaddr_in *sockaddr, const char *address, in_port_t port);

int tcp_socket(const char *address, in_port_t port)
{
    int ISETOPTION = 1;

    int                sockfd;
    struct sockaddr_in sockaddr;

    // Create TCP Socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd < 0)
    {
        fprintf(stderr, "tcp_socket::socket: Invalid socket file descriptor generated.\n");
        sockfd = -1;
        goto exit;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&ISETOPTION, sizeof(ISETOPTION));    // Allows for rebinding to address after non-graceful termination

    // Setup address and port for socket binding
    memset(&sockaddr, 0, sizeof(struct sockaddr_in));
    setup_addr(&sockaddr, address, port);

    // Bind the socket
    errno = 0;
    if(bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
    {
        perror("tcp_socket::bind");
        close(sockfd);
        sockfd = -2;
        goto exit;
    }

    // Enable client connections
    errno = 0;
    if(listen(sockfd, SOMAXCONN) < 0)
    {
        perror("tcp_socket::listen");
        close(sockfd);
        sockfd = -3;
        goto exit;
    }

exit:
    return sockfd;
}

/**
 * Sets up an IPv4 address in a socket address struct.
 */
void setup_addr(struct sockaddr_in *sockaddr, const char *address, in_port_t port)
{
    sockaddr->sin_addr.s_addr = inet_addr(address);
    sockaddr->sin_family      = AF_INET;
    sockaddr->sin_port        = htons(port);
}

in_port_t convert_port(const char *str, int *err)
{
    in_port_t port;
    char     *endptr;
    long      val;

    *err  = 0;
    port  = 0;
    errno = 0;
    val   = strtol(str, &endptr, 10);    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    // Check if no digits were found
    if(endptr == str)
    {
        *err = -1;
        goto done;
    }

    // Check for out-of-range errors
    if(val < 0 || val > UINT16_MAX)
    {
        *err = -2;
        goto done;
    }

    // Check for trailing invalid characters
    if(*endptr != '\0')
    {
        *err = -3;
        goto done;
    }

    port = (in_port_t)val;

done:
    return port;
}
