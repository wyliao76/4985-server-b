// cppcheck-suppress-file unusedStructMember

#ifndef MESSAGING_H
#define MESSAGING_H

#include "fsm.h"
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define HEADER_SIZE 6
#define RESPONSE_SIZE 256

typedef enum
{
    ONE   = 0x01,
    TWO   = 0x02,
    THREE = 0x03,
} version_t;

typedef enum
{
    BOOLEAN         = 0x01,
    INTEGER         = 0x02,
    null            = 0x05,
    ENUMERATED      = 0x0A,
    UTF8STRING      = 0x0C,
    SEQUENCE        = 0x10,
    SEQUENCEOF      = 0x30,
    PrintableString = 0x13,
    UTCTime         = 0x17,
    GeneralizedTime = 0x18,
} tag_t;

typedef enum
{
    // 0
    OK = 0x00,
    // 11
    INVALID_USER_ID = 0x0B,
    // 12
    INVALID_AUTH = 0x0C,
    // 13
    USER_EXISTS = 0x0D,
    // 21
    SERVER_ERROR = 0x15,
    // 31
    INVALID_REQUEST = 0x1F,
    // 32
    REQUEST_TIMEOUT = 0x20,
} code_t;

typedef enum
{
    // 0
    SYS_Success = 0x00,
    // 1
    SYS_Error = 0x01,
    // 10
    ACC_Login = 0x0A,
    // 11
    ACC_Login_Success = 0x0B,
    // 12
    ACC_Logout = 0x0C,
    // 13
    ACC_Create = 0x0D,
    // 14
    ACC_Edit = 0x0E,
    // 20
    CHT_Send = 0x14,
    // 21
    CHT_Received = 0x15,
    // 30
    LST_Get = 0x1E,
    // 31
    LST_Response = 0x1F
} type_t;

// typedef struct header_t
// {
//     uint8_t  type;
//     uint8_t  version;
//     uint16_t sender_id;
//     uint16_t payload_len;
// } header_t;

// typedef struct body_t
// {
//     uint8_t *msg;
// } body_t;

// typedef struct res_body_t
// {
//     uint8_t  tag;
//     uint8_t  len;
//     uint8_t  value;
//     uint8_t  msg_tag;
//     uint8_t  msg_len;
//     uint8_t *msg;
// } res_body_t;

// typedef struct acc_t
// {
//     uint8_t  username_tag;
//     uint8_t  username_len;
//     uint8_t *username;
//     uint8_t  password_tag;
//     uint8_t  password_len;
//     uint8_t *password;
// } acc_t;

typedef struct request_t
{
    void    *content;
    size_t   len;
    int     *err;
    int     *client_fd;
    uint16_t sender_id;
    uint8_t  type;
    code_t  *code;
    // void    *response;
    uint8_t  response[RESPONSE_SIZE];
    uint16_t payload_len;
} request_t;

typedef struct codeMapping
{
    code_t      code;
    const char *msg;
} codeMapping;

typedef struct funcMapping
{
    type_t type;
    ssize_t (*func)(request_t *request);
} funcMapping;

typedef struct user_count_t
{
    uint8_t  type;
    uint8_t  version;
    uint16_t payload_len;
    uint8_t  tag;
    uint8_t  len;
    uint16_t value;
} user_count_t;

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */

const char *code_to_string(const code_t *code);

void event_loop(int server_fd, int *err);

fsm_state_t request_handler(void *args);

fsm_state_t header_handler(void *args);

fsm_state_t body_handler(void *args);

fsm_state_t process_handler(void *args);

fsm_state_t response_handler(void *args);

fsm_state_t error_handler(void *args);

// ssize_t read_packet(int fd, uint8_t **buf, request_t *request, response_t *response, int *err);

// ssize_t deserialize_header(header_t *header, response_t *response, const uint8_t *buf, ssize_t nread);

// ssize_t deserialize_body(request_t *request, response_t *response, const uint8_t *buf, ssize_t nread, int *err);

// ssize_t serialize_header(const response_t *response, uint8_t *buf);

// ssize_t serialize_body(const response_t *response, uint8_t *buf);

// ssize_t create_response(const request_t *request, response_t *response, uint8_t **buf, size_t *response_len, int *err);

// ssize_t sent_response(int fd, const uint8_t *buf, const size_t *response_len);

#endif
