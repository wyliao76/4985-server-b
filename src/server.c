#include "args.h"
#include "fsm.h"
#include "messaging.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
// #include <unistd.h>

#define INADDRESS "0.0.0.0"
#define OUTADDRESS "127.0.0.1"
// #define BACKLOG 5
#define PORT "8081"
#define SM_PORT "8082"

int main(int argc, char *argv[])
{
    int retval;
    // int            sockfd;
    // int            sm_fd;
    args_t args;
    // int            err;
    fsm_args_t     fsm_args;
    fsm_state_func perform;
    fsm_state_t    from_id;
    fsm_state_t    to_id;

    static struct fsm_transition transitions[] = {
        {START,        SETUP_SIGNAL,  setup_signal },
        {SETUP_SIGNAL, GET_ARGUMENTS, get_arguments},
        {ERROR,        EXIT,          NULL         },
    };

    // const unsigned char sm_msg[] = {
    //     ACC_Login,    // 10
    //     0x01,         // 1
    //     0x00,         // 0
    //     0x04,         // 4
    //     0x02,         // 2
    //     0x02,         // 2
    //     0x00,         // 0
    //     ACC_Login,    // 10
    // };

    printf("Server launching... (press Ctrl+C to interrupt)\n");

    memset(&args, 0, sizeof(args_t));
    args.addr = INADDRESS;
    convert_port(PORT, &args.port);
    args.sm_addr = OUTADDRESS;
    convert_port(SM_PORT, &args.sm_port);

    memset(&args, 0, sizeof(fsm_args_t));
    fsm_args.args = &args;
    fsm_args.argc = argc;
    fsm_args.argv = argv;

    retval = EXIT_SUCCESS;

    from_id = START;
    to_id   = SETUP_SIGNAL;

    // do
    // {
    //     perform = fsm_transition(from_id, to_id, transitions, sizeof(transitions));
    // if(perform == NULL)
    // {
    //     goto exit;
    // }
    //     to_id = perform(&fsm_args);
    // } while(to_id != EXIT);

    perform = fsm_transition(from_id, to_id, transitions, sizeof(transitions));
    if(perform == NULL)
    {
        goto exit;
    }
    else
    {
        from_id = to_id;
        to_id   = perform(&fsm_args);
    }

    printf("%d\n", to_id);
    printf("%d\n", to_id);

    perform = fsm_transition(from_id, to_id, transitions, sizeof(transitions));
    if(perform == NULL)
    {
        goto exit;
    }
    else
    {
        // from_id = to_id;
        perform(&args);
    }

    // // Start TCP Server
    // sockfd = tcp_server(&args.addr, &args.port, BACKLOG, &err);
    // if(sockfd < 0)
    // {
    //     fprintf(stderr, "main::tcp_server: Failed to create TCP server.\n");
    //     return EXIT_FAILURE;
    // }

    // printf("Listening on %s:%d\n", args.addr, args.port);

    // // Start TCP Client
    // sm_fd = tcp_client(&args.sm_addr, &args.sm_port, &err);
    // // if(sm_fd < 0)
    // // {
    // //     fprintf(stderr, "main::tcp_server: Failed to create TCP server.\n");
    // //     return EXIT_FAILURE;
    // // }

    // printf("Connect to server manager at %s:%d\n", args.sm_addr, args.sm_port);

    // // just for demo
    // if(write(sm_fd, sm_msg, sizeof(user_count_t)) < 0)
    // {
    //     fprintf(stderr, "main::tcp_client: write to server manager failed.\n");
    //     // return EXIT_FAILURE;
    // }

    // // Wait for client connections
    // err     = 0;
    // running = 1;
    while(running)
    {
        //     int                connfd;
        //     struct sockaddr_in connaddr;
        //     socklen_t          connsize;

        //     // !!BLOCKING!! Get client connection
        //     connsize = sizeof(struct sockaddr_in);
        //     memset(&connaddr, 0, connsize);

        //     errno  = 0;
        //     connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connsize);
        //     if(connfd < 0)
        //     {
        //         // perror("main::accept");
        //         continue;
        //     }

        //     printf("New connection from: %s:%d\n", inet_ntoa(connaddr.sin_addr), connaddr.sin_port);

        //     // Fork the process
        //     errno = 0;
        //     pid   = fork();
        //     if(pid < 0)
        //     {
        //         perror("main::fork");
        //         close(connfd);
        //         continue;
        //     }

        //     if(pid == 0)
        //     {
        //         retval = (int)request_handler(connfd);
        //         close(connfd);
        //         goto exit;
        //     }

        //     close(connfd);
    }

exit:
    // close(sm_fd);
    // close(sockfd);
    return retval;
}
