#include "messaging.h"
#include "database.h"
#include "io.h"
#include "networking.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LEN 64
#define TIMEOUT 3000    // 3s
// testing
#define MSG_COUNT 0x00000064
uint16_t user_count = 0;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
uint32_t msg_count  = MSG_COUNT;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
int      user_index = 0;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

static const char *code_to_string(const code_t *code);
static ssize_t     check_len(const uint8_t *len);
static ssize_t     account_create(request_t *request);
static ssize_t     account_login(request_t *request);
static ssize_t     account_logout(request_t *request);
static ssize_t     chat_broadcast(request_t *request);
static ssize_t     execute_functions(request_t *request, const funcMapping functions[]);
static void        serialize_sm_diagnostic(char *msg);
static void        count_user(const int *sessions);
static void        send_user_count(int sm_fd, char *msg, int *err);
static void        error_response(request_t *request);
static fsm_state_t request_handler(void *args);
static fsm_state_t header_handler(void *args);
static fsm_state_t body_handler(void *args);
static fsm_state_t process_handler(void *args);
static fsm_state_t response_handler(void *args);
static fsm_state_t error_handler(void *args);

static const codeMapping code_map[] = {
    {OK,              ""                                  },
    {INVALID_USER_ID, "Invalid User ID"                   },
    {INVALID_AUTH,    "Invalid Authentication Information"},
    {USER_EXISTS,     "User Already exist"                },
    {SERVER_ERROR,    "Server Error"                      },
    {INVALID_REQUEST, "Invalid Request"                   },
    {REQUEST_TIMEOUT, "Request Timeout"                   }
};

static const char *code_to_string(const code_t *code)
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

static ssize_t check_len(const uint8_t *len)
{
    if(*len > MAX_LEN || *len == 0)
    {
        printf("invalid len\n");
        return -1;
    }
    return 0;
}

static const struct fsm_transition transitions[] = {
    {START,            REQUEST_HANDLER,  request_handler },
    {REQUEST_HANDLER,  HEADER_HANDLER,   header_handler  },
    {HEADER_HANDLER,   BODY_HANDLER,     body_handler    },
    {HEADER_HANDLER,   PROCESS_HANDLER,  process_handler },
    {BODY_HANDLER,     PROCESS_HANDLER,  process_handler },
    {PROCESS_HANDLER,  RESPONSE_HANDLER, response_handler},
    {RESPONSE_HANDLER, END,              NULL            },
    {REQUEST_HANDLER,  ERROR_HANDLER,    error_handler   },
    {HEADER_HANDLER,   ERROR_HANDLER,    error_handler   },
    {BODY_HANDLER,     ERROR_HANDLER,    error_handler   },
    {PROCESS_HANDLER,  ERROR_HANDLER,    error_handler   },
    {ERROR_HANDLER,    END,              NULL            },
};

const funcMapping acc_func[] = {
    {ACC_Create,  account_create},
    {ACC_Login,   account_login },
    {ACC_Logout,  account_logout},
    {ACC_Edit,    NULL          },
    {CHT_Send,    chat_broadcast},
    {SYS_Success, NULL          }  // Null termination for safety
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
    printf("Not builtin command: %d\n", *(uint8_t *)request->content);
    return 1;
}

static ssize_t account_create(request_t *request)
{
    DBO         userDB;
    DBO         index_userDB;
    char        user_name[]  = "users";
    char        index_name[] = "index_user";
    void       *existing;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;

    // server default to 0
    uint16_t sender_id = SERVER_ID;

    userDB.name       = user_name;
    userDB.db         = NULL;
    index_userDB.name = index_name;
    index_userDB.db   = NULL;

    printf("in account_create %d \n", request->client->fd);

    if(database_open(&userDB, &request->err) < 0)
    {
        perror("database error");
        request->code = SERVER_ERROR;
        goto error;
    }

    if(database_open(&index_userDB, &request->err) < 0)
    {
        perror("database error");
        request->code = SERVER_ERROR;
        goto error;
    }

    // start from username len
    ptr = (char *)request->content + HEADER_SIZE + 1;

    memcpy(&user_len, ptr, sizeof(user_len));
    printf("user_len: %d\n", user_len);
    if(check_len(&user_len) == -1)
    {
        request->code = INVALID_REQUEST;
        goto error;
    }
    ptr += sizeof(user_len);

    username = ptr;

    // start from password len
    ptr += user_len + 1;
    memcpy(&pass_len, ptr, sizeof(pass_len));
    printf("pass_len: %d\n", pass_len);
    if(check_len(&pass_len) == -1)
    {
        request->code = INVALID_REQUEST;
        goto error;
    }
    ptr += sizeof(pass_len);

    password = ptr;

    printf("username: %.*s\n", (int)user_len, username);
    printf("password: %.*s\n", (int)pass_len, password);

    // check user exists
    existing = retrieve_byte(userDB.db, username, user_len);
    if(existing)
    {
        printf("Retrieved password: %.*s\n", (int)user_len, (char *)existing);
        request->code = USER_EXISTS;
        free(existing);
        goto error;
    }

    user_index++;
    *request->session_id = user_index;

    printf("user_index: %d\n", user_index);
    printf("request->session_id: %d\n", *request->session_id);

    // Store user
    if(store_byte(userDB.db, username, user_len, password, pass_len) != 0)
    {
        perror("store_byte");
        request->code = SERVER_ERROR;
        goto error;
    }

    // Store user index
    if(store_byte(index_userDB.db, username, user_len, request->session_id, sizeof(int)) < 0)
    {
        perror("update user_index");
        request->code = SERVER_ERROR;
        goto error;
    }

    ptr = (char *)request->response;
    // tag
    *ptr++ = SYS_Success;
    // version
    *ptr++ = VERSION;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    // payload len
    request->response_len = htons(request->response_len);
    memcpy(ptr, &request->response_len, sizeof(request->response_len));
    ptr += sizeof(request->response_len);

    *ptr++ = ENUMERATED;
    *ptr++ = sizeof(uint8_t);
    *ptr++ = ACC_Create;

    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    return 0;

error:
    dbm_close(userDB.db);
    dbm_close(index_userDB.db);

    return -1;
}

static ssize_t account_login(request_t *request)
{
    DBO         userDB;
    DBO         index_userDB;
    char        user_name[]  = "users";
    char        index_name[] = "index_user";
    datum       output;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;
    int        *user_id;
    ssize_t     result;

    // server default to 0
    uint16_t sender_id = SERVER_ID;

    userDB.name       = user_name;
    userDB.db         = NULL;
    index_userDB.name = index_name;
    index_userDB.db   = NULL;

    printf("in account_login %d \n", request->client->fd);

    memset(&output, 0, sizeof(datum));

    if(database_open(&userDB, &request->err) < 0)
    {
        perror("database error");
        request->code = SERVER_ERROR;
        goto error;
    }

    if(database_open(&index_userDB, &request->err) < 0)
    {
        perror("database error");
        request->code = SERVER_ERROR;
        goto error;
    }

    printf("extracting user_len\n");
    // start from username len
    ptr = (char *)request->content + HEADER_SIZE + 1;

    memcpy(&user_len, ptr, sizeof(user_len));
    printf("user_len: %d\n", user_len);
    if(check_len(&user_len) == -1)
    {
        request->code = INVALID_REQUEST;
        goto error;
    }
    ptr += sizeof(user_len);

    username = ptr;

    printf("extracting pass_len\n");
    // start from password len
    ptr += user_len + 1;
    memcpy(&pass_len, ptr, sizeof(pass_len));
    printf("pass_len: %d\n", pass_len);
    if(check_len(&pass_len) == -1)
    {
        request->code = INVALID_REQUEST;
        goto error;
    }
    ptr += sizeof(pass_len);

    password = ptr;

    printf("username: %.*s\n", (int)user_len, username);
    printf("password: %.*s\n", (int)pass_len, password);

    result = verify_user(userDB.db, username, user_len, password, pass_len);
    if(result == -1)
    {
        printf("user not exist\n");
        request->code = INVALID_USER_ID;
        goto error;
    }
    else if(result == -2)
    {
        printf("password invalid length\n");
        request->code = INVALID_AUTH;
        goto error;
    }
    else if(result == -3)
    {
        printf("invalid auth\n");
        request->code = INVALID_AUTH;
        goto error;
    }

    user_id = (int *)retrieve_byte(index_userDB.db, username, user_len);
    if(!user_id)
    {
        printf("account login retrieve_int error\n");
        request->code = SERVER_ERROR;
        goto error;
    }
    printf("account login: user_id: %.*d\n", (int)sizeof(*request->session_id), *user_id);

    memset(request->response, 0, RESPONSE_SIZE);

    ptr = (char *)request->response;
    // tag
    *ptr++ = ACC_Login_Success;
    // version
    *ptr++ = VERSION;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    // payload len
    request->response_len = 4;
    request->response_len = htons(request->response_len);
    memcpy(ptr, &request->response_len, sizeof(request->response_len));
    ptr += sizeof(request->response_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(uint16_t);

    *user_id = htons(*(uint16_t *)user_id);
    memcpy(ptr, user_id, sizeof(*user_id));

    *request->session_id = ntohs(*(uint16_t *)user_id);

    printf("session_id %d\n", *request->session_id);

    free(user_id);
    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    return 0;

error:
    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    return -1;
}

static ssize_t account_logout(request_t *request)
{
    PRINT_VERBOSE("in account_logout %d \n", request->client->fd);
    PRINT_DEBUG("sender_id %d \n", request->sender_id);
    PRINT_DEBUG("session_id %d \n", *request->session_id);

    if(request->sender_id != *request->session_id)
    {
        request->code = INVALID_REQUEST;
        return -1;
    }

    request->response_len = 0;

    request->err = 0;
    return -1;
}

static ssize_t chat_broadcast(request_t *request)
{
    char       *ptr;
    const char *timestamp;
    const char *content;
    const char *username;
    uint8_t     time_len;
    uint8_t     content_len;
    uint8_t     user_len;

    PRINT_VERBOSE("in chat_broadcast %d \n", request->client->fd);
    PRINT_DEBUG("sender_id %d \n", request->sender_id);
    PRINT_DEBUG("session_id %d \n", *request->session_id);

    if(request->sender_id != *request->session_id)
    {
        request->code = INVALID_REQUEST;
        return -1;
    }

    // broadcast
    // start from timestamp len
    ptr = (char *)request->content + HEADER_SIZE + 1;

    memcpy(&time_len, ptr, sizeof(time_len));
    ptr += sizeof(time_len);

    timestamp = ptr;

    // start from content len
    ptr += time_len + 1;
    memcpy(&content_len, ptr, sizeof(content_len));
    ptr += sizeof(content_len);

    content = ptr;

    // start from user len
    ptr += content_len + 1;
    memcpy(&user_len, ptr, sizeof(user_len));
    ptr += sizeof(user_len);

    username = ptr;

    printf("timestamp: %.*s\n", (int)time_len, timestamp);
    printf("content: %.*s\n", (int)content_len, content);
    printf("username: %.*s\n", (int)user_len, username);

    request->response_len = (uint16_t)(HEADER_SIZE + request->len);
    printf("response_len: %d\n", request->response_len);
    memcpy(request->response, request->content, request->response_len);

    msg_count++;
    printf("msg_count: %d\n", (int)msg_count);

    for(int i = 1; i < MAX_FDS; i++)
    {
        if(request->fds[i].fd != -1)
        {
            printf("broadcasting... %d\n", request->fds[i].fd);
            write_fully(request->fds[i].fd, request->response, request->response_len, &request->err);
        }
    }

    request->response_len = 0;

    return 0;
}

static void serialize_sm_diagnostic(char *msg)
{
    char    *ptr;
    uint16_t msg_payload_len = htons(MSG_PAYLOAD_LEN);

    ptr = msg;

    *ptr++ = SVR_Diagnostic;
    *ptr++ = VERSION;
    memcpy(ptr, &msg_payload_len, sizeof(msg_payload_len));
    ptr += sizeof(msg_payload_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(user_count);
    ptr += sizeof(user_count);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(msg_count);
}

static void count_user(const int *sessions)
{
    // printf("user_index: %d\n", user_index);
    user_count = 0;
    for(int i = 1; i < MAX_FDS; i++)
    {
        // printf("user id: %d\n", sessions[i]);
        if(sessions[i] != -1)
        {
            user_count++;
        }
    }
    printf("user_count: %d\n", user_count);
}

static void send_user_count(int sm_fd, char *msg, int *err)
{
    char *ptr;

    ptr = msg;
    // move 6 bytes
    ptr += SM_HEADER_SIZE + 1 + 1;
    user_count = htons(user_count);
    memcpy(ptr, &user_count, sizeof(user_count));
    ptr += sizeof(user_count) + 1 + 1;

    msg_count = htonl(msg_count);
    memcpy(ptr, &msg_count, sizeof(msg_count));
    msg_count = ntohl(msg_count);

    printf("send_user_count\n");

    if(write_fully(sm_fd, msg, MSG_LEN, err) < 0)
    {
        perror("send_user_count failed");
        errno = 0;
    }
}

static void error_response(request_t *request)
{
    char *ptr;

    // server default to 0
    uint16_t    sender_id = SERVER_ID;
    const char *msg;
    uint8_t     msg_len;

    ptr = (char *)request->response;
    // tag
    *ptr++ = SYS_Error;
    // version
    *ptr++ = VERSION;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    msg     = code_to_string(&request->code);
    msg_len = (uint8_t)strlen(msg);

    request->response_len = (uint16_t)(request->response_len + (sizeof(uint8_t) + sizeof(uint8_t) + msg_len));

    // payload len
    request->response_len = htons(request->response_len);
    memcpy(ptr, &request->response_len, sizeof(request->response_len));
    ptr += sizeof(request->response_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(uint8_t);

    memcpy(ptr, &request->code, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    *ptr++ = UTF8STRING;
    memcpy(ptr, &msg_len, sizeof(msg_len));
    ptr += sizeof(msg_len);

    memcpy(ptr, msg, msg_len);
}

void event_loop(int server_fd, int sm_fd, int *err)
{
    struct pollfd fds[MAX_FDS];
    // user_ids
    int     sessions[MAX_FDS];
    int     client_fd;
    int     added;
    char    db_name[] = "meta_user";
    DBO     meta_userDB;
    ssize_t result;
    char    msg[MSG_LEN];

    meta_userDB.name = db_name;

    if(init_pk(&meta_userDB, USER_PK) < 0)
    {
        perror("init_pk error\n");
        goto cleanup;
    }

    if(database_open(&meta_userDB, err) < 0)
    {
        perror("database error");
        goto cleanup;
    }

    serialize_sm_diagnostic(msg);

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
        printf("polling...\n");
        result = poll(fds, MAX_FDS, TIMEOUT);
        // printf("result %d\n", (int)result);
        if(result == -1)
        {
            if(errno == EINTR)
            {
                goto cleanup;
            }
            perror("Poll error");
            goto cleanup;
        }
        if(result == 0)
        {
            printf("syncing meta_user...\n");

            // update user index
            if(store_int(meta_userDB.db, USER_PK, user_index) != 0)
            {
                perror("update user_index");
                goto cleanup;
            }
            count_user(sessions);
            send_user_count(sm_fd, msg, err);
            continue;
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

                    errno          = 0;
                    request.err    = 0;
                    request.client = &fds[i];
                    // user_id
                    request.session_id   = &sessions[i];
                    request.len          = HEADER_SIZE;
                    request.response_len = 3;
                    request.fds          = fds;
                    request.content      = malloc(CONTENT_SIZE);
                    if(request.content == NULL)
                    {
                        perror("Malloc failed to allocate memory\n");
                        close(fds[i].fd);
                        fds[i].fd   = -1;
                        sessions[i] = -1;
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
                            close(fds[i].fd);
                            fds[i].fd     = -1;
                            fds[i].events = 0;
                            sessions[i]   = -1;
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
                    fds[i].fd     = -1;
                    fds[i].events = 0;
                    sessions[i]   = -1;
                    continue;
                }
            }
        }
    }

    printf("syncing meta_user...\n");
    // update user index
    if(store_int(meta_userDB.db, USER_PK, user_index) != 0)
    {
        perror("update user_index");
    }
    dbm_close(meta_userDB.db);
    return;

cleanup:
    printf("syncing meta_user in cleanup...\n");
    store_int(meta_userDB.db, USER_PK, user_index);
    dbm_close(meta_userDB.db);
}

static fsm_state_t request_handler(void *args)
{
    request_t *request;
    ssize_t    nread;

    request = (request_t *)args;
    printf("in request_handler %d\n", request->client->fd);

    if(setSocketNonBlocking(request->client->fd, &request->err) == -1)
    {
        request->code = SERVER_ERROR;
        return ERROR_HANDLER;
    }

    // Read first 6 bytes from fd
    errno = 0;
    nread = read_fully(request->client->fd, (char *)request->content, request->len, &request->err);
    printf("request_handler nread %d\n", (int)nread);
    if(nread < 0)
    {
        perror("Read_fully error");
        request->code = SERVER_ERROR;
        return ERROR_HANDLER;
    }
    // idk what state should it be so just to illegal state
    if(nread == 0)
    {
        PRINT_VERBOSE("%s\n", "request_handler getting 0 byte");
        request->code = OK;
        return RESPONSE_HANDLER;
    }

    if(nread < (ssize_t)request->len)
    {
        request->code = INVALID_REQUEST;
        return ERROR_HANDLER;
    }
    return HEADER_HANDLER;
}

static fsm_state_t header_handler(void *args)
{
    request_t *request;

    uint16_t sender_id;
    uint16_t len;
    char    *ptr;

    request = (request_t *)args;

    printf("in header_handler %d\n", request->client->fd);

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

    if(request->len == 0)
    {
        return PROCESS_HANDLER;
    }

    if(request->len > CONTENT_SIZE - HEADER_SIZE)
    {
        printf("invalid payload len\n");
        request->code = INVALID_REQUEST;
        return ERROR_HANDLER;
    }

    return BODY_HANDLER;
}

static fsm_state_t body_handler(void *args)
{
    request_t *request;
    ssize_t    nread;

    request = (request_t *)args;
    printf("in header_handler %d\n", request->client->fd);

    printf("len size: %u\n", (uint16_t)(request->len + HEADER_SIZE));

    nread = read_fully(request->client->fd, (char *)request->content + HEADER_SIZE, request->len, &request->err);
    if(nread < 0)
    {
        perror("Read_fully error\n");
        request->code = SERVER_ERROR;
        return ERROR_HANDLER;
    }

    return PROCESS_HANDLER;
}

static fsm_state_t process_handler(void *args)
{
    request_t *request;
    ssize_t    result;

    request = (request_t *)args;

    printf("in process_handler %d\n", request->client->fd);

    result = execute_functions(request, acc_func);
    if(result == 0)
    {
        return RESPONSE_HANDLER;
    }
    if(result < 0)
    {
        return ERROR_HANDLER;
    }

    request->code = INVALID_REQUEST;
    return ERROR_HANDLER;
}

static fsm_state_t response_handler(void *args)
{
    request_t *request;
    const char log[] = "./text.txt";

    request = (request_t *)args;

    printf("in response_handler %d\n", request->client->fd);

    if(request->type != CHT_Send)
    {
        int fd;

        request->response_len = (uint16_t)(HEADER_SIZE + ntohs(request->response_len));
        printf("response_len: %d\n", (request->response_len));

        // testing
        fd = open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if(fd == -1)
        {
            perror("open");
            errno = 0;
        }
        else
        {
            write_fully(fd, request->response, request->response_len, &request->err);
            close(fd);
        }

        write_fully(request->client->fd, request->response, request->response_len, &request->err);
    }

    memset(request->content, 0, CONTENT_SIZE);
    free(request->content);
    return END;
}

static fsm_state_t error_handler(void *args)
{
    request_t *request;
    const char log[] = "./text.txt";

    request = (request_t *)args;

    printf("in error_handler %d: %d\n", request->client->fd, (int)request->code);

    if(request->type != ACC_Logout)
    {
        error_response(request);
        request->response_len = (uint16_t)(HEADER_SIZE + ntohs(request->response_len));
    }
    printf("response_len: %d\n", (request->response_len));

    // don't write if connection was gone
    if(errno != ECONNRESET)
    {
        // testing
        int fd;
        fd = open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if(fd == -1)
        {
            perror("open");
            errno = 0;
        }
        else
        {
            write_fully(fd, request->response, request->response_len, &request->err);
            close(fd);
        }
        write_fully(request->client->fd, request->response, request->response_len, &request->err);
    }

    memset(request->content, 0, CONTENT_SIZE);
    free(request->content);
    return END;
}
