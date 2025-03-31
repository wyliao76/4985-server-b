#include "starter.h"
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

#define SLEEP 3    // 3s
#define INADDRESS "0.0.0.0"
#define OUTADDRESS "0.0.0.0"
#define PORT "8081"
#define SM_PORT "9000"

static const struct fsm_transition transitions[] = {
    {START,           CONNECT_SM,      connect_sm     },
    {CONNECT_SM,      PARSE_ENVP,      parse_envp     },
    {PARSE_ENVP,      WAIT_FOR_START,  wait_for_start },
    {WAIT_FOR_START,  LAUNCH_SERVER,   launch_server  },
    {LAUNCH_SERVER,   CLEANUP_HANDLER, cleanup_handler},
    {CLEANUP_HANDLER, END,             NULL           },
    {CONNECT_SM,      CLEANUP_HANDLER, cleanup_handler},
    {PARSE_ENVP,      CLEANUP_HANDLER, cleanup_handler},
    {WAIT_FOR_START,  CLEANUP_HANDLER, cleanup_handler},
    {LAUNCH_SERVER,   WAIT_FOR_START,  wait_for_start },
    {WAIT_FOR_START,  CONNECT_SM,      connect_sm     }
};

static ssize_t send_online(void *args)
{
    args_t  *sm_args     = (args_t *)args;
    uint16_t payload_len = htons(0x0000);
    char    *ptr;

    memset((void *)sm_args->buf, 0, SM_HEADER_SIZE);

    ptr    = sm_args->buf;
    *ptr++ = SVR_Online;
    *ptr++ = VERSION;
    memcpy(ptr, &payload_len, sizeof(payload_len));

    return write_fully(*sm_args->sm_fd, sm_args->buf, SM_HEADER_SIZE, sm_args->err);
}

static ssize_t send_offline(void *args)
{
    args_t  *sm_args     = (args_t *)args;
    uint16_t payload_len = htons(0x0000);
    char    *ptr;

    memset((void *)sm_args->buf, 0, SM_HEADER_SIZE);

    ptr    = sm_args->buf;
    *ptr++ = SVR_Offline;
    *ptr++ = VERSION;
    memcpy(ptr, &payload_len, sizeof(payload_len));

    return write_fully(*sm_args->sm_fd, sm_args->buf, SM_HEADER_SIZE, sm_args->err);
}

static void cleanup(void *args)
{
    args_t *sm_args = (args_t *)args;

    for(int i = 0; sm_args->envp[i] != NULL; i++)
    {
        free(sm_args->envp[i]);
    }
    for(int i = 0; sm_args->argv[i] != NULL; i++)
    {
        free(sm_args->argv[i]);
    }
}

fsm_state_t connect_sm(void *args)
{
    args_t *sm_args = (args_t *)args;
    while(running)
    {
        // Start TCP Client
        PRINT_VERBOSE("%s\n", "Connecting to server manager...");
        *sm_args->sm_fd = tcp_client(sm_args->sm_addr, sm_args->sm_port, sm_args->err);
        PRINT_VERBOSE("sm_fd: %d\n", *sm_args->sm_fd);
        if(*sm_args->sm_fd > 0)
        {
            PRINT_VERBOSE("Connect to server manager at %s:%d\n", sm_args->sm_addr, sm_args->sm_port);
            return PARSE_ENVP;
        }
        perror("Failed to create TCP client.");
        errno         = 0;
        *sm_args->err = 0;
        sleep(SLEEP);
    }
    return CLEANUP_HANDLER;
}

fsm_state_t wait_for_start(void *args)
{
    struct pollfd fds[1];
    ssize_t       nread;

    args_t *sm_args = (args_t *)args;

    PRINT_DEBUG("%s\n", "wait_for_start");

    memset(sm_args->buf, 0, BUF_SIZE);

    fds[0].fd     = *sm_args->sm_fd;
    fds[0].events = POLLIN;

    while(running)
    {
        if(poll(fds, 1, -1) == -1)
        {
            perror("Poll error");
            return CLEANUP_HANDLER;
        }

        if(fds[0].revents & POLLIN)
        {
            nread = read_fully(fds[0].fd, sm_args->buf, SM_HEADER_SIZE, sm_args->err);
            if(nread < 0 || nread < SM_HEADER_SIZE)
            {
                PRINT_VERBOSE("%s\n", "read_fully error in wait_for_start. Closing connection...");
                errno         = 0;
                *sm_args->err = 0;
                close(fds[0].fd);
                sleep(SLEEP);
                return CONNECT_SM;
            }

            // print
            for(int i = 0; i < SM_HEADER_SIZE; i++)
            {
                PRINT_DEBUG("%d ", sm_args->buf[i]);
            }
            PRINT_DEBUG("%s\n", "");

            if(sm_args->buf[0] == SVR_Start)
            {
                PRINT_VERBOSE("%s\n", "Server start requested");
                return LAUNCH_SERVER;
            }
        }
        if(fds[0].revents & (POLLHUP | POLLERR))
        {
            PRINT_VERBOSE("%s\n", "server manager hung up");
            close(fds[0].fd);
            sleep(SLEEP);
            return CONNECT_SM;
        }
    }
    return CLEANUP_HANDLER;
}

fsm_state_t parse_envp(void *args)
{
    args_t     *sm_args = (args_t *)args;
    int         index;
    const char *program_name = "./build/server";

    // Anonymous struct + inline initialization
    struct
    {
        const char *key;
        const void *value;
        int         is_string;    // 1 for string, 0 for in_port_t, 2 for int
    } const env_vars[] = {
        {"ADDR=",    sm_args->addr,     1},
        {"PORT=",    &sm_args->port,    0},
        {"SM_ADDR=", sm_args->sm_addr,  1},
        {"SM_PORT=", &sm_args->sm_port, 0},
        {"SM_FD=",   sm_args->sm_fd,    2},
        {"VERBOSE=", &verbose,          2},
        {NULL,       NULL,              0}  // End marker
    };

    cleanup(sm_args);
    index = 0;

    for(int i = 0; env_vars[i].key != NULL; i++)
    {
        if(env_vars[i].is_string == 1)
        {
            snprintf(sm_args->buf, BUF_SIZE, "%s%s", env_vars[i].key, (const char *)env_vars[i].value);
        }
        else if(env_vars[i].is_string == 2)
        {
            snprintf(sm_args->buf, BUF_SIZE, "%s%d", env_vars[i].key, *(const int *)env_vars[i].value);
        }
        else
        {
            snprintf(sm_args->buf, BUF_SIZE, "%s%u", env_vars[i].key, *(const in_port_t *)env_vars[i].value);
        }

        sm_args->envp[index] = strdup(sm_args->buf);
        if(sm_args->envp[index] == NULL)
        {
            perror("strdup failed");
            return CLEANUP_HANDLER;
        }
        memset(sm_args->buf, 0, BUF_SIZE);
        index++;
    }
    sm_args->envp[index] = NULL;

    sm_args->argv[0] = strdup(program_name);
    if(sm_args->argv[0] == NULL)
    {
        perror("strdup failed");
        return CLEANUP_HANDLER;
    }
    sm_args->argv[1] = NULL;

    for(int i = 0; sm_args->envp[i] != NULL; i++)
    {
        PRINT_DEBUG("%s\n", sm_args->envp[i]);
    }

    return WAIT_FOR_START;
}

fsm_state_t launch_server(void *args)
{
    int           status;
    pid_t         pid;
    struct pollfd fds[1];

    args_t *sm_args = (args_t *)args;

    pid = fork();

    if(pid < 0)
    {
        perror("main::fork");
        *sm_args->err = errno;
        return CLEANUP_HANDLER;
    }

    if(pid == 0)
    {
        execve(sm_args->argv[0], sm_args->argv, sm_args->envp);

        // _exit if fails
        perror("execv failed");
        _exit(EXIT_FAILURE);
    }

    if(send_online(sm_args) < 0)
    {
        return CLEANUP_HANDLER;
    }

    fds[0].fd     = *sm_args->sm_fd;
    fds[0].events = POLLIN;

    while(running)
    {
        ssize_t result;

        errno  = 0;
        result = poll(fds, 1, -1);
        if(result == -1)
        {
            perror("Poll error");
            if(errno == EINTR)
            {
                errno = 0;
                // server died accidentally, don't kill starter
                if(running == 1)
                {
                    PRINT_VERBOSE("%s\n", "checking child...");
                    if(waitpid(pid, &status, WNOHANG) > 0)
                    {
                        PRINT_VERBOSE("Child exited, status = %d\n", WEXITSTATUS(status));
                        send_offline(sm_args);
                        return WAIT_FOR_START;
                    }
                }
            }
            else
            {
                *sm_args->err = errno;
            }
            return CLEANUP_HANDLER;
        }
        if(fds[0].revents & POLLIN)
        {
            memset(sm_args->buf, 0, SM_HEADER_SIZE);

            if(read_fully(fds[0].fd, sm_args->buf, SM_HEADER_SIZE, sm_args->err) < 0)
            {
                perror("read_fully error");
                continue;
            }

            if(sm_args->buf[0] == SVR_Stop)
            {
                PRINT_VERBOSE("%s\n", "Killing child process...");
                // print
                for(int i = 0; i < SM_HEADER_SIZE; i++)
                {
                    PRINT_DEBUG("%d ", sm_args->buf[i]);
                }
                PRINT_DEBUG("%s\n", "");

                kill(pid, SIGINT);
                waitpid(pid, &status, 0);    // Ensure child cleanup
                send_offline(sm_args);
                return WAIT_FOR_START;
            }
        }
        if(fds[0].revents & (POLLHUP | POLLERR))
        {
            PRINT_VERBOSE("%s\n", "server manager exit");
            PRINT_VERBOSE("%s\n", "Killing child process...");
            // print
            for(int i = 0; i < SM_HEADER_SIZE; i++)
            {
                PRINT_DEBUG("%d ", sm_args->buf[i]);
            }
            PRINT_DEBUG("%s\n", "");

            kill(pid, SIGINT);
            waitpid(pid, &status, 0);    // Ensure child cleanup
            return CLEANUP_HANDLER;
        }
    }
    kill(pid, SIGINT);
    waitpid(pid, &status, 0);    // Ensure child cleanup
    return CLEANUP_HANDLER;
}

fsm_state_t cleanup_handler(void *args)
{
    args_t *sm_args = (args_t *)args;

    send_offline(sm_args);

    cleanup(sm_args);

    close(*sm_args->sm_fd);

    return END;
}

int main(int argc, char *argv[])
{
    int            err;
    args_t         args;
    int            sm_fd;
    fsm_state_func perform;
    fsm_state_t    from_id;
    fsm_state_t    to_id;

    setup_signal(0);

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

    printf("verbose: %d\n", verbose);
    PRINT_VERBOSE("%s\n", "verbose on");
    PRINT_DEBUG("%s\n", "debug on");

    from_id = START;
    to_id   = CONNECT_SM;

    do
    {
        perform = fsm_transition(from_id, to_id, transitions, sizeof(transitions));
        if(perform == NULL)
        {
            PRINT_VERBOSE("illegal state %d, %d \n", from_id, to_id);
            close(sm_fd);
            for(int i = 0; args.envp[i] != NULL; i++)
            {
                free(args.envp[i]);
            }
            for(int i = 0; args.argv[i] != NULL; i++)
            {
                free(args.argv[i]);
            }
            goto error;
        }
        // printf("from_id %d\n", from_id);
        from_id = to_id;
        to_id   = perform(&args);
    } while(to_id != END);

    PRINT_VERBOSE("%s\n", "Starter shutting down...");

    if(err != 0)
    {
        perror("There was an error...");
        goto error;
    }
    return EXIT_SUCCESS;

error:
    return EXIT_FAILURE;
}
