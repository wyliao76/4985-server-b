#include "messaging.h"
#include "account.h"
#include "io.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLIENTS 2
#define MAX_FDS (MAX_CLIENTS + 1)

static ssize_t execute_functions(request_t *request, const funcMapping functions[]);

static const codeMapping code_map[] = {
    {OK,              ""                                  },
    {INVALID_USER_ID, "Invalid User ID"                   },
    {INVALID_AUTH,    "Invalid Authentication Information"},
    {USER_EXISTS,     "User Already exist"                },
    {SERVER_ERROR,    "Server Error"                      },
    {INVALID_REQUEST, "Invalid Request"                   },
    {REQUEST_TIMEOUT, "Request Timeout"                   }
};

const char *code_to_string(const code_t *code)
{
    for(size_t i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++)
    {
        if(code_map[i].code == *code)
        {
            return code_map[i].msg;
        }
    }
    return "UNKNOWN_STATUS";
}

static const struct fsm_transition transitions[] = {
    {START,            REQUEST_HANDLER,  request_handler },
    {REQUEST_HANDLER,  HEADER_HANDLER,   header_handler  },
    {HEADER_HANDLER,   BODY_HANDLER,     body_handler    },
    {BODY_HANDLER,     PROCESS_HANDLER,  process_handler },
    {PROCESS_HANDLER,  RESPONSE_HANDLER, response_handler},
    {RESPONSE_HANDLER, END,              NULL            },
    {REQUEST_HANDLER,  ERROR_HANDLER,    error_handler   },
    {HEADER_HANDLER,   ERROR_HANDLER,    error_handler   },
    {BODY_HANDLER,     ERROR_HANDLER,    error_handler   },
    {PROCESS_HANDLER,  ERROR_HANDLER,    error_handler   },
    {ERROR_HANDLER,    END,              NULL            },
};

static ssize_t execute_functions(request_t *request, const funcMapping functions[])
{
    for(size_t i = 0; functions[i].type != SYS_Success; i++)
    {
        if(request->type == functions[i].type)
        {
            return functions[i].func(request);
        }
    }
    printf("Not builtin command: %d\n", *(uint16_t *)request->content);
    return 1;
}

void event_loop(int server_fd, int *err)
{
    struct pollfd fds[MAX_FDS];
    int           sessions[MAX_FDS];
    int           client_fd;
    int           added;

    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;
    for(int i = 1; i < MAX_FDS; i++)
    {
        fds[i].fd   = -1;
        sessions[i] = -1;
    }

    while(running)
    {
        errno = 0;
        if(poll(fds, MAX_FDS, -1) < 0)
        {
            if(errno == EINTR)
            {
                goto cleanup;
            }
            perror("Poll error");
            goto cleanup;
        }

        // Check for new connection
        if(fds[0].revents & POLLIN)
        {
            client_fd = accept(server_fd, NULL, 0);
            if(client_fd < 0)
            {
                if(errno == EINTR)
                {
                    goto cleanup;
                }
                perror("Accept failed");
                continue;
            }

            // Add new client to poll list
            added = 0;
            for(int i = 1; i < MAX_FDS; i++)
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
                write_fully(client_fd, &too_many, (ssize_t)strlen(too_many), err);

                close(client_fd);
                continue;
            }
        }

        // Check existing clients for data
        for(int i = 1; i < MAX_FDS; i++)
        {
            if(fds[i].fd != -1)
            {
                if(fds[i].revents & POLLIN)
                {
                    request_t      request;
                    fsm_state_func perform;
                    fsm_state_t    from_id;
                    fsm_state_t    to_id;

                    from_id = START;
                    to_id   = REQUEST_HANDLER;

                    request.err          = 0;
                    request.client_fd    = &fds[i].fd;
                    request.session_id   = &sessions[i];
                    request.len          = HEADER_SIZE;
                    request.response_len = 3;
                    request.content      = malloc(HEADER_SIZE);
                    if(request.content == NULL)
                    {
                        perror("Malloc failed to allocate memory\n");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        continue;
                    }

                    memset(request.response, 0, RESPONSE_SIZE);

                    request.code = OK;

                    printf("event_loop session_id %d\n", *request.session_id);

                    do
                    {
                        perform = fsm_transition(from_id, to_id, transitions, sizeof(transitions));
                        if(perform == NULL)
                        {
                            printf("illegal state %d, %d \n", from_id, to_id);
                            free(request.content);
                            close(*request.client_fd);
                            *request.client_fd = -1;
                            break;
                        }
                        // printf("from_id %d\n", from_id);
                        from_id = to_id;
                        to_id   = perform(&request);
                    } while(to_id != END);
                }
                if(fds[i].revents & (POLLHUP | POLLERR))
                {
                    // Client disconnected or error, close and clean up
                    printf("oops...\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    continue;
                }
            }
        }
    }

cleanup:
    return;
}

fsm_state_t request_handler(void *args)
{
    request_t *request;
    ssize_t    nread;

    request = (request_t *)args;
    printf("in request_handler %d\n", *request->client_fd);

    // Read first 6 bytes from fd
    errno = 0;
    nread = read_fully(*request->client_fd, (char *)request->content, request->len, &request->err);
    if(nread < 0)
    {
        perror("Read_fully error\n");
        return ERROR_HANDLER;
    }

    if(nread < (ssize_t)request->len)
    {
        request->code = INVALID_REQUEST;
        return ERROR_HANDLER;
    }
    return HEADER_HANDLER;
}

fsm_state_t header_handler(void *args)
{
    request_t *request;

    uint16_t sender_id;
    uint16_t len;
    char    *ptr;

    request = (request_t *)args;

    printf("in header_handler %d\n", *request->client_fd);

    ptr = (char *)request->content;

    memcpy(&request->type, ptr, sizeof(request->type));
    ptr += sizeof(request->type) + sizeof(uint8_t);

    memcpy(&sender_id, ptr, sizeof(sender_id));
    request->sender_id = ntohs(sender_id);
    ptr += sizeof(sender_id);

    printf("sender_id: %u\n", request->sender_id);

    memcpy(&len, ptr, sizeof(len));
    // printf("len size (before ntohs): %u\n", len);
    request->len = ntohs(len);
    printf("len size (after ntohs): %u\n", (uint16_t)request->len);

    return BODY_HANDLER;
}

fsm_state_t body_handler(void *args)
{
    request_t *request;
    ssize_t    nread;
    void      *buf;

    request = (request_t *)args;
    printf("in header_handler %d\n", *request->client_fd);

    printf("len size: %u\n", (uint16_t)(request->len + HEADER_SIZE));

    buf = realloc(request->content, request->len + HEADER_SIZE);
    if(!buf)
    {
        perror("Failed to realloc buf");
        return ERROR_HANDLER;
    }
    request->content = buf;

    nread = read_fully(*request->client_fd, (char *)request->content + HEADER_SIZE, request->len, &request->err);
    if(nread < 0)
    {
        perror("Read_fully error\n");
        return ERROR_HANDLER;
    }

    return PROCESS_HANDLER;
}

fsm_state_t process_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;

    printf("in process_handler %d\n", *request->client_fd);

    if(execute_functions(request, acc_func) < 0)
    {
        return ERROR_HANDLER;
    }
    return RESPONSE_HANDLER;
}

fsm_state_t response_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;

    printf("in response_handler %d\n", *request->client_fd);
    printf("response_len: %d\n", (request->response_len));

    write_fully(*request->client_fd, request->response, request->response_len, &request->err);

    free(request->content);
    return END;
}

fsm_state_t error_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;
    printf("in error_handler %d: %d\n", *request->client_fd, (int)request->code);
    printf("response_len: %d\n", (request->response_len));

    write_fully(*request->client_fd, request->response, request->response_len, &request->err);

    free(request->content);
    close(*request->client_fd);
    *request->client_fd = -1;
    return END;
}
