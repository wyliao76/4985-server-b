#include "account.h"
#include "database.h"
#include "messaging.h"
#include <arpa/inet.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <string.h>

const funcMapping acc_func[] = {
    {ACC_Create,  account_create},
    {ACC_Login,   account_login },
    {ACC_Logout,  account_logout},
    {ACC_Edit,    NULL          },
    {SYS_Success, NULL          }  // Null termination for safety
};

ssize_t account_create(request_t *request)
{
    char        db_name[] = "mydb";
    ssize_t     result;
    DBO         dbo;
    void       *existing;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;
    uint16_t    sender_id = 0x0000;
    const char *msg;
    uint8_t     msg_len;

    printf("in account_create %d \n", *request->client_fd);

    dbo.name = db_name;

    result = database_open(&dbo, request->err);
    if(result == -1)
    {
        perror("database error");
        *request->code = SERVER_ERROR;
        goto error;
    }

    // start from username len
    ptr = (char *)request->content + HEADER_SIZE + 1;

    memcpy(&user_len, ptr, sizeof(user_len));
    ptr += sizeof(user_len);

    username = ptr;

    // start from password len
    ptr += user_len + 1;
    memcpy(&pass_len, ptr, sizeof(pass_len));
    ptr += sizeof(pass_len);

    password = ptr;

    printf("username: %.*s\n", (int)user_len, username);
    printf("password: %.*s\n", (int)pass_len, password);

    // check user exists
    existing = retrieve_byte(dbo.db, username, user_len);

    if(existing)
    {
        printf("Retrieved password: %.*s\n", (int)pass_len, (char *)existing);
        *request->code = USER_EXISTS;
        free(existing);
        goto error;
    }

    // Store user
    if(store_byte(dbo.db, username, user_len, password, pass_len) != 0)
    {
        perror("user");
        *request->code = SERVER_ERROR;
        goto error;
    }

    ptr = (char *)request->response;
    // tag
    *ptr++ = SYS_Success;
    // version
    *ptr++ = TWO;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    // payload len
    request->payload_len = htons(request->payload_len);
    memcpy(ptr, &request->payload_len, sizeof(request->payload_len));
    ptr += sizeof(request->payload_len);

    *ptr++ = ENUMERATED;
    *ptr++ = sizeof(uint8_t);
    *ptr++ = ACC_Create;

    dbm_close(dbo.db);
    return 0;

error:
    ptr = (char *)request->response;
    // tag
    *ptr++ = SYS_Error;
    // version
    *ptr++ = TWO;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    msg     = code_to_string(request->code);
    msg_len = (uint8_t)strlen(msg);

    request->payload_len = (uint16_t)(request->payload_len + (sizeof(uint8_t) + sizeof(uint8_t) + msg_len));

    // payload len
    request->payload_len = htons(request->payload_len);
    memcpy(ptr, &request->payload_len, sizeof(request->payload_len));
    ptr += sizeof(request->payload_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(uint8_t);

    memcpy(ptr, request->code, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    *ptr++ = UTF8STRING;
    memcpy(ptr, &msg_len, sizeof(msg_len));
    ptr += sizeof(msg_len);

    memcpy(ptr, msg, msg_len);

    dbm_close(dbo.db);
    return -1;
}

ssize_t account_login(request_t *request)
{
    char        db_name[] = "mydb";
    ssize_t     result;
    DBO         dbo;
    void       *existing;
    datum       output;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;
    uint16_t    sender_id = 0x0000;
    const char *msg;
    uint8_t     msg_len;
    uint16_t    user_id = 1;

    printf("in account_login %d \n", *request->client_fd);

    memset(&output, 0, sizeof(datum));

    dbo.name = db_name;

    result = database_open(&dbo, request->err);
    if(result == -1)
    {
        perror("database error");
        *request->code = SERVER_ERROR;
        goto error;
    }

    // start from username len
    ptr = (char *)request->content + HEADER_SIZE + 1;

    memcpy(&user_len, ptr, sizeof(user_len));
    ptr += sizeof(user_len);

    username = ptr;

    // start from password len
    ptr += user_len + 1;
    memcpy(&pass_len, ptr, sizeof(pass_len));
    ptr += sizeof(pass_len);

    password = ptr;

    printf("username: %.*s\n", (int)user_len, username);
    printf("password: %.*s\n", (int)pass_len, password);

    // check user exists
    existing = retrieve_byte(dbo.db, username, user_len);
    if(!existing)
    {
        perror("Username not found");
        *(request->code) = INVALID_USER_ID;
        goto error;
    }

    printf("Retrieved password: %.*s\n", (int)pass_len, (char *)existing);

    if(memcmp(existing, password, pass_len) != 0)
    {
        free(existing);
        *(request->code) = INVALID_AUTH;
        goto error;
    }

    ptr = (char *)request->response;
    // tag
    *ptr++ = ACC_Login_Success;
    // version
    *ptr++ = TWO;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    // payload len
    request->payload_len = 4;
    request->payload_len = htons(request->payload_len);
    memcpy(ptr, &request->payload_len, sizeof(request->payload_len));
    ptr += sizeof(request->payload_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(uint16_t);

    user_id = htons(user_id);
    memcpy(ptr, &user_id, sizeof(user_id));

    dbm_close(dbo.db);
    free(existing);
    return 0;

error:
    ptr = (char *)request->response;
    // tag
    *ptr++ = SYS_Error;
    // version
    *ptr++ = TWO;

    // sender_id
    sender_id = htons(sender_id);
    memcpy(ptr, &sender_id, sizeof(sender_id));
    ptr += sizeof(sender_id);

    msg     = code_to_string(request->code);
    msg_len = (uint8_t)strlen(msg);

    request->payload_len = (uint16_t)(request->payload_len + (sizeof(uint8_t) + sizeof(uint8_t) + msg_len));

    // payload len
    request->payload_len = htons(request->payload_len);
    memcpy(ptr, &request->payload_len, sizeof(request->payload_len));
    ptr += sizeof(request->payload_len);

    *ptr++ = INTEGER;
    *ptr++ = sizeof(uint8_t);

    memcpy(ptr, request->code, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    *ptr++ = UTF8STRING;
    memcpy(ptr, &msg_len, sizeof(msg_len));
    ptr += sizeof(msg_len);

    memcpy(ptr, msg, msg_len);

    dbm_close(dbo.db);
    return -1;
}

ssize_t account_logout(request_t *request)
{
    printf("in account_logout %d \n", *request->client_fd);

    request->err = 0;
    return -1;
}
