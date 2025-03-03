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

// #define ERR_READ (-5)
#define MAX_CLIENTS 2
#define MAX_FDS (MAX_CLIENTS + 1)
#define HEADER_SIZE 6

static ssize_t execute_functions(request_t *request, const funcMapping functions[]);

// static const codeMapping code_map[] = {
//     {OK,              ""                                  },
//     {INVALID_USER_ID, "Invalid User ID"                   },
//     {INVALID_AUTH,    "Invalid Authentication Information"},
//     {USER_EXISTS,     "User Already exist"                },
//     {SERVER_ERROR,    "Server Error"                      },
//     {INVALID_REQUEST, "Invalid Request"                   },
//     {REQUEST_TIMEOUT, "Request Timeout"                   }
// };

// static const char *code_to_string(const code_t *code)
// {
//     for(size_t i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++)
//     {
//         if(code_map[i].code == *code)
//         {
//             return code_map[i].msg;
//         }
//     }
//     return "UNKNOWN_STATUS";
// }

static const struct fsm_transition transitions[] = {
    {START,           REQUEST_HANDLER, request_handler},
    {REQUEST_HANDLER, HEADER_HANDLER,  header_handler },
    {HEADER_HANDLER,  BODY_HANDLER,    body_handler   },
    {BODY_HANDLER,    PROCESS_HANDLER, process_handler},
    {PROCESS_HANDLER, CLEANUP,         cleanup_handler},
    {REQUEST_HANDLER, ERROR,           error_handler  },
    {HEADER_HANDLER,  ERROR,           error_handler  },
    {BODY_HANDLER,    ERROR,           error_handler  },
    {CLEANUP,         END,             NULL           },
    {ERROR,           END,             NULL           },
};

static ssize_t execute_functions(request_t *request, const funcMapping functions[])
{
    for(size_t i = 0; functions[i].type != SYS_Success; i++)
    {
        // if(*(uint8_t *)request->content == (uint8_t)functions[i].type)
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
    int           client_fd;
    int           added;

    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;
    for(int i = 1; i < MAX_FDS; i++)
    {
        fds[i].fd = -1;
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
                if(fds[i].revents & (POLLHUP | POLLERR))
                {
                    // Client disconnected or error, close and clean up
                    printf("oops...\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    continue;
                }
                if(fds[i].revents & POLLIN)
                {
                    request_t      request;
                    fsm_state_func perform;
                    fsm_state_t    from_id;
                    fsm_state_t    to_id;

                    from_id = START;
                    // from_id           = ERROR;
                    to_id             = REQUEST_HANDLER;
                    request.err       = err;
                    request.client_fd = &fds[i].fd;
                    request.len       = HEADER_SIZE;
                    request.content   = malloc(HEADER_SIZE);
                    if(request.content == NULL)
                    {
                        perror("Malloc failed to allocate memory\n");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        continue;
                    }

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

    request       = (request_t *)args;
    request->code = OK;
    printf("in request_handler %d\n", *request->client_fd);

    // Read first 6 bytes from fd
    errno = 0;
    nread = read_fully(*request->client_fd, (char *)request->content, request->len, request->err);
    if(nread < 0)
    {
        perror("Read_fully error\n");
        return ERROR;
    }

    if(nread < (ssize_t)request->len)
    {
        request->code = INVALID_REQUEST;
        return ERROR;
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

    buf = malloc(request->len + HEADER_SIZE);
    if(!buf)
    {
        perror("Failed to allocate buf");
        return ERROR;
    }
    free(request->content);
    request->content = buf;
    nread            = read_fully(*request->client_fd, (char *)request->content + HEADER_SIZE, request->len, request->err);
    if(nread < 0)
    {
        perror("Read_fully error\n");
        return ERROR;
    }

    return PROCESS_HANDLER;
}

fsm_state_t process_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;

    printf("in process_handler %d\n", *request->client_fd);

    execute_functions(request, acc_func);
    return CLEANUP;
}

fsm_state_t cleanup_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;

    printf("in cleanup_handler %d\n", *request->client_fd);

    free(request->content);

    // temp
    close(*request->client_fd);
    *request->client_fd = -1;
    return END;
}

fsm_state_t error_handler(void *args)
{
    request_t *request;

    request = (request_t *)args;
    printf("in error_handler %d\n", *request->client_fd);
    free(request->content);
    close(*request->client_fd);
    *request->client_fd = -1;
    return END;
}

// ssize_t deserialize_body(request_t *request, response_t *response, const uint8_t *buf, ssize_t nread, int *err)
// {
//     if(nread < (ssize_t)request->header->payload_len)
//     {
//         printf("body too short\n");
//         *err              = EINVAL;
//         *(response->code) = INVALID_REQUEST;
//         return -1;
//     }

//     printf("type: %d\n", (int)request->header->type);

//     if(request->header->type == ACC_Create || request->header->type == ACC_Login)
//     {
//         size_t offset;
//         acc_t *acc;

//         acc = (acc_t *)malloc(sizeof(acc_t));
//         if(!acc)
//         {
//             perror("Failed to allocate acc_t");
//             *err              = errno;
//             *(response->code) = SERVER_ERROR;
//             return -1;
//         }
//         memset(acc, 0, sizeof(acc_t));

//         offset = 0;
//         // Deserialize username tag
//         memcpy(&acc->username_tag, buf + offset, sizeof(acc->username_tag));
//         offset += sizeof(acc->username_tag);

//         // printf("tag: %d\n", (int)acc->username_tag);

//         // Deserialize username length
//         memcpy(&acc->username_len, buf + offset, sizeof(acc->username_len));
//         offset += sizeof(acc->username_len);

//         // printf("len: %d\n", (int)acc->username_len);

//         // Deserialize username
//         acc->username = (uint8_t *)malloc((size_t)acc->username_len + 1);
//         if(!acc->username)
//         {
//             perror("Failed to allocate username");
//             free(acc);
//             *err              = errno;
//             *(response->code) = SERVER_ERROR;
//             return -1;
//         }
//         memcpy(acc->username, buf + offset, acc->username_len);
//         acc->username[acc->username_len] = '\0';
//         offset += acc->username_len;

//         // printf("username: %s\n", acc->username);

//         // Deserialize username tag
//         memcpy(&acc->password_tag, buf + offset, sizeof(acc->password_tag));
//         offset += sizeof(acc->password_tag);

//         // printf("password_tag: %d\n", (int)acc->password_tag);

//         // Deserialize password length
//         memcpy(&acc->password_len, buf + offset, sizeof(acc->password_len));
//         offset += sizeof(acc->password_len);

//         // printf("password_len: %d\n", (int)acc->password_len);

//         // Deserialize password
//         acc->password = (uint8_t *)malloc((size_t)acc->password_len + 1);
//         if(!acc->password)
//         {
//             perror("Failed to allocate password");
//             free(acc->username);
//             free(acc);
//             *err              = errno;
//             *(response->code) = SERVER_ERROR;
//             return -1;
//         }
//         memcpy(acc->password, buf + offset, acc->password_len);
//         acc->password[acc->password_len] = '\0';

//         free(request->body);
//         request->body = (body_t *)acc;

//         // printf("password: %s\n", acc->password);

//         return 0;
//     }

//     printf("Unknown type: 0x%X\n", request->header->type);
//     *(response->code) = INVALID_REQUEST;
//     return -1;
// }

// ssize_t serialize_header(const response_t *response, uint8_t *buf)
// {
//     size_t offset;

//     offset = 0;

//     memcpy(buf + offset, &response->header->type, sizeof(response->header->type));
//     offset += sizeof(response->header->type);

//     memcpy(buf + offset, &response->header->version, sizeof(response->header->version));
//     offset += sizeof(response->header->version);

//     response->header->sender_id = htons(response->header->sender_id);
//     memcpy(buf + offset, &response->header->sender_id, sizeof(response->header->sender_id));
//     offset += sizeof(response->header->sender_id);

//     response->header->payload_len = htons(response->header->payload_len);
//     memcpy(buf + offset, &response->header->payload_len, sizeof(response->header->payload_len));

//     return 0;
// }

// ssize_t serialize_body(const response_t *response, uint8_t *buf)
// {
//     size_t offset;

//     offset = 0;

//     memcpy(buf + offset, &response->body->tag, sizeof(response->body->tag));
//     offset += sizeof(response->body->tag);

//     memcpy(buf + offset, &response->body->len, sizeof(response->body->len));
//     offset += sizeof(response->body->len);

//     memcpy(buf + offset, &response->body->value, sizeof(response->body->value));
//     offset += sizeof(response->body->value);

//     printf("%d\n", (int)offset);
//     response->header->payload_len = htons(response->header->payload_len);
//     printf("%d\n", (int)response->header->payload_len);

//     if(response->header->payload_len > offset)
//     {
//         memcpy(buf + offset, &response->body->msg_tag, sizeof(response->body->msg_tag));
//         offset += sizeof(response->body->msg_tag);

//         memcpy(buf + offset, &response->body->msg_len, sizeof(response->body->msg_len));
//         offset += sizeof(response->body->msg_len);

//         memcpy(buf + offset, code_to_string(response->code), response->header->payload_len - offset);
//     }

//     return 0;
// }

// ssize_t create_response(const request_t *request, response_t *response, uint8_t **buf, size_t *response_len, int *err)
// {
//     uint8_t    *newbuf;
//     const char *msg;

//     printf("response_len before: %d\n", (int)*response_len);

//     *response_len = (size_t)(request->header_len + response->header->payload_len);
//     printf("response_len: %d\n", (int)*response_len);

//     *buf = (uint8_t *)malloc(*response_len);
//     if(*buf == NULL)
//     {
//         perror("read_packet::realloc");
//         *(response->code) = SERVER_ERROR;
//         *err              = errno;
//         return -1;
//     }

//     msg = code_to_string(response->code);

//     // tag
//     response->body->msg_tag = (uint8_t)UTF8STRING;
//     // len
//     response->body->msg_len = (uint8_t)(strlen(msg));
//     printf("msg_len: %d\n", (int)response->body->msg_len);
//     // msg

//     *response_len = *response_len + response->body->msg_len + 2;
//     printf("response_len final: %d\n", (int)*response_len);

//     if(*response->code == OK)
//     {
//         response->header->type      = ACC_Login_Success;
//         response->header->version   = ONE;
//         response->header->sender_id = 0x00;
//         // logout has no response body
//         if(request->header->type == ACC_Logout)
//         {
//             response->header->payload_len = 0;
//             *response_len                 = *response_len - 2 - 3;
//         }
//         else
//         {
//             // ok has no msg
//             response->header->payload_len = (uint16_t)3;
//             // no msg tag & msg len
//             *response_len -= 2;
//         }
//     }
//     else if(*response->code == INVALID_AUTH)
//     {
//         printf("*response->code == INVALID_AUTH\n");

//         response->header->type        = SYS_Error;
//         response->header->version     = ONE;
//         response->header->sender_id   = 0x00;
//         response->header->payload_len = (uint16_t)(3 + response->body->msg_len + 2);

//         newbuf = (uint8_t *)realloc(*buf, *response_len);
//         if(newbuf == NULL)
//         {
//             perror("read_packet::realloc");
//             *(response->code) = SERVER_ERROR;
//             return -4;
//         }
//         *buf = newbuf;
//     }
//     else
//     {
//         printf("*response->code == else \n");
//         response->header->type        = SYS_Error;
//         response->header->version     = ONE;
//         response->header->sender_id   = 0x00;
//         response->header->payload_len = (uint16_t)(3 + response->body->msg_len + 2);

//         newbuf = (uint8_t *)realloc(*buf, *response_len);
//         if(newbuf == NULL)
//         {
//             perror("read_packet::realloc");
//             *(response->code) = SERVER_ERROR;
//             return -4;
//         }
//         *buf = newbuf;
//     }

//     serialize_header(response, *buf);
//     serialize_body(response, *buf + sizeof(header_t));
//     return 0;
// }

// ssize_t sent_response(int fd, const uint8_t *buf, const size_t *response_len)
// {
//     ssize_t result;

//     result = write(fd, buf, *response_len);
//     printf("\n%d\n", (int)result);
//     return 0;
// }
