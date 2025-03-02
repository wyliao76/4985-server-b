#include "args.h"
#include "networking.h"
#include <getopt.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>

#define UNKNOWN_OPTION_MESSAGE_LEN 22

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
    fputs("  -A <sm address>, --sm address <sm address>  The address of server manager.\n", stderr);
    fputs("  -P <sm port>,    --sm port <sm port>        The server manager port.\n", stderr);
    exit(exit_code);
}

fsm_state_t get_arguments(void *args)
{
    int         opt;
    fsm_args_t *fsm_args;

    static struct option long_options[] = {
        {"address",    required_argument, NULL, 'a'},
        {"port",       required_argument, NULL, 'p'},
        {"sm address", required_argument, NULL, 'A'},
        {"sm_port",    required_argument, NULL, 'P'},
        {"help",       no_argument,       NULL, 'h'},
        {NULL,         0,                 NULL, 0  }
    };

    fsm_args = (fsm_args_t *)args;

    printf("in get_arguments\n");

    while((opt = getopt_long(fsm_args->argc, fsm_args->argv, "ha:p:A:P:", long_options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'a':
                fsm_args->args->addr = optarg;
                break;
            case 'p':
                if(convert_port(optarg, &fsm_args->args->port) != 0)
                {
                    // usage(fsm_args->argv[0], EXIT_FAILURE, "Port must be between 1 and 65535");
                    fprintf(stderr, "Port must be between 0 to 65535\n");
                    return ERROR;    // port not valid
                }
                break;
            case 'A':
                fsm_args->args->sm_addr = optarg;
                break;
            case 'P':
                if(convert_port(optarg, &fsm_args->args->sm_port) != 0)
                {
                    // usage(fsm_args->argv[0], EXIT_FAILURE, "Port must be between 1 and 65535");
                    fprintf(stderr, "Port must be between 0 to 65535\n");
                    return ERROR;    // port not valid
                }
                break;
            case 'h':
                usage(fsm_args->argv[0], EXIT_SUCCESS, NULL);
            case '?':
                if(optopt != 'a' && optopt != 'p' && optopt != 'A' && optopt != 'P')
                {
                    char message[UNKNOWN_OPTION_MESSAGE_LEN];

                    snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                    usage(fsm_args->argv[0], EXIT_FAILURE, message);
                }
                break;
            default:
                usage(fsm_args->argv[0], EXIT_FAILURE, NULL);
        }
    }
    return EXIT;
}
