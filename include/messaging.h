// cppcheck-suppress-file unusedStructMember

#ifndef MESSAGING_H
#define MESSAGING_H

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */

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
    INVALID_USER_ID = 0x0B,
    INVALID_AUTH    = 0x0C,
    USER_EXISTS     = 0x0D,
    INVALID_REQUEST = 0x1F,
    REQUEST_TIMEOUT = 0x20,
} code_t;

typedef enum
{
    ACC_CREATE = 0x0D,
    ACC_LOGIN  = 0x0A,
} type_t;

typedef struct header_t
{
    uint8_t  type;
    uint8_t  version;
    uint16_t sender_id;
    uint16_t payload_len;
} header_t;

typedef struct body_t
{
    uint8_t _dummy;
} body_t;

typedef struct acc_t
{
    body_t   base;
    uint8_t  username_tag;
    uint8_t  username_len;
    uint8_t *username;
    uint8_t  password_tag;
    uint8_t  password_len;
    uint8_t *password;
} acc_t;

typedef struct request_t
{
    header_t *header;
    body_t   *body;
} request_t;

typedef struct response_t
{
    header_t *header;
    code_t   *code;
    body_t   *body;
} response_t;

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */

ssize_t request_handler(int connfd);

ssize_t read_packet(int fd, uint8_t **buf, request_t *request, int *err);

ssize_t deserialize_header(header_t *header, const uint8_t *buf, ssize_t nread);

ssize_t deserialize_body(request_t *request, const uint8_t *buf, ssize_t nread, int *err);

#endif
