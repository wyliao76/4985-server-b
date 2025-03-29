#include "account.h"
#include "database.h"
#include "utils.h"
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

// static ssize_t secure_cmp(const void *a, const void *b, size_t size);

// static ssize_t secure_cmp(const void *a, const void *b, size_t size)
// {
//     const uint8_t *x    = (const uint8_t *)a;
//     const uint8_t *y    = (const uint8_t *)b;
//     uint8_t        diff = 0;

//     for(size_t i = 0; i < size; i++)
//     {
//         diff |= x[i] ^ y[i];    // XOR accumulates differences
//     }

//     return diff;    // 0 means equal, nonzero means different
// }

ssize_t account_create(request_t *request)
{
    DBO         userDB;
    DBO         index_userDB;
    char        user_name[]  = "users";
    char        index_name[] = "index_user";
    char       *existing;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;
    char       *copy_username;
    char       *copy_password;
    int         user_id;

    // server default to 0
    uint16_t sender_id = SERVER_ID;

    userDB.name       = user_name;
    userDB.db         = NULL;
    index_userDB.name = index_name;
    index_userDB.db   = NULL;

    copy_username = NULL;
    copy_password = NULL;

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
    ptr += sizeof(user_len);

    username = ptr;

    // start from password len
    ptr += user_len + 1;
    memcpy(&pass_len, ptr, sizeof(pass_len));
    ptr += sizeof(pass_len);

    password = ptr;

    printf("username: %.*s\n", (int)user_len, username);
    printf("password: %.*s\n", (int)pass_len, password);

    copy_username = strndup(username, user_len);
    if(!copy_username)
    {
        perror("Failed to allocate memory");
        request->code = SERVER_ERROR;
        goto error;
    }

    copy_password = strndup(password, pass_len);
    if(!copy_password)
    {
        perror("Failed to allocate memory");
        request->code = SERVER_ERROR;
        goto error;
    }

    // check user exists
    existing = retrieve_string(userDB.db, copy_username);
    if(existing)
    {
        printf("Retrieved password: %s\n", existing);
        request->code = USER_EXISTS;
        free(existing);
        goto error;
    }

    user_index++;
    *request->session_id = user_index;

    printf("user_index: %d\n", user_index);
    printf("request->session_id: %d\n", *request->session_id);

    // Store user
    if(store_string(userDB.db, copy_username, copy_password) != 0)
    {
        perror("store_byte");
        request->code = SERVER_ERROR;
        goto error;
    }

    // Store user index
    if(store_int(index_userDB.db, copy_username, *request->session_id) < 0)
    {
        perror("update user_index");
        request->code = SERVER_ERROR;
        goto error;
    }

    // for checking
    if(retrieve_int(index_userDB.db, copy_username, &user_id) < 0)
    {
        printf("account account retrieve_int error\n");
        request->code = SERVER_ERROR;
        goto error;
    }
    printf("account login: user_id: %.*d\n", (int)sizeof(*request->session_id), user_id);

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
    free(copy_username);
    free(copy_password);
    return 0;

error:
    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    free(copy_username);
    free(copy_password);

    return -1;
}

ssize_t account_login(request_t *request)
{
    DBO         userDB;
    DBO         index_userDB;
    char        user_name[]  = "users";
    char        index_name[] = "index_user";
    char       *existing;
    datum       output;
    uint8_t     user_len;
    uint8_t     pass_len;
    char       *ptr;
    const char *username;
    const char *password;
    char       *copy_username;
    char       *copy_password;
    int         user_id;

    // server default to 0
    uint16_t sender_id = SERVER_ID;

    userDB.name       = user_name;
    userDB.db         = NULL;
    index_userDB.name = index_name;
    index_userDB.db   = NULL;

    copy_username = NULL;
    copy_password = NULL;

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

    copy_username = strndup(username, user_len);
    if(!copy_username)
    {
        perror("Failed to allocate memory");
        request->code = SERVER_ERROR;
        goto error;
    }

    copy_password = strndup(password, pass_len);
    if(!copy_password)
    {
        perror("Failed to allocate memory");
        request->code = SERVER_ERROR;
        goto error;
    }

    // check user exists
    existing = retrieve_string(userDB.db, copy_username);
    if(!existing)
    {
        perror("Username not found");
        request->code = INVALID_USER_ID;
        goto error;
    }

    PRINT_VERBOSE("Retrieved password: %s\n", existing);
    PRINT_VERBOSE("copy_password: %s\n", copy_password);

    printf("%d\n", (int)strlen(existing));

    if(strcmp(existing, copy_password) != 0)
    // if(secure_cmp(existing, copy_password, strlen(existing)) != 0)
    {
        free(existing);
        request->code = INVALID_AUTH;
        goto error;
    }

    if(retrieve_int(index_userDB.db, copy_username, &user_id) < 0)
    {
        printf("account login retrieve_int error\n");
        free(existing);
        request->code = SERVER_ERROR;
        goto error;
    }
    printf("account login: user_id: %.*d\n", (int)sizeof(*request->session_id), user_id);

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

    user_id = htons((uint16_t)user_id);
    memcpy(ptr, &user_id, sizeof(user_id));

    *request->session_id = ntohs((uint16_t)user_id);

    printf("session_id %d\n", *request->session_id);

    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    free(existing);
    free(copy_username);
    free(copy_password);
    return 0;

error:
    dbm_close(userDB.db);
    dbm_close(index_userDB.db);
    free(copy_username);
    free(copy_password);
    return -1;
}

ssize_t account_logout(request_t *request)
{
    printf("in account_logout %d \n", request->client->fd);

    request->response_len = 0;

    request->err = 0;
    return -1;
}
