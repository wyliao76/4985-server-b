#include "account.h"
#include "database.h"
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

ssize_t account_create(const request_t *request)
{
    printf("in account_create %d \n", *request->client_fd);
    return 0;
}

ssize_t account_login(const request_t *request)
{
    printf("in account_login %d \n", *request->client_fd);
    return 0;
    //     char        *password;
    //     char         db_name[] = "mydb";
    //     ssize_t      result;
    //     DBO          dbo;
    //     datum        output;
    //     const acc_t *acc = (acc_t *)request->body;

    //     memset(&output, 0, sizeof(datum));
    //     dbo.name = db_name;

    //     printf("account_login\n");

    //     printf("username: %s\n", acc->username);
    //     printf("password: %s\n", acc->password);

    //     result = database_open(&dbo, err);

    //     if(result == -1)
    //     {
    //         perror("database error");
    //         *(response->code) = SERVER_ERROR;
    //         goto error;
    //     }

    //     // check user exists
    //     password = retrieve_string(dbo.db, (char *)acc->username);
    //     if(!password)
    //     {
    //         perror("Username not found");
    //         *(response->code) = INVALID_USER_ID;
    //         goto error;
    //     }
    //     printf("Retrieved password: %s\n", password);

    //     if(strcmp(password, (char *)acc->password) != 0)
    //     {
    //         free((void *)password);
    //         *(response->code) = INVALID_AUTH;
    //         goto error;
    //     }

    //     response->body->tag = INTEGER;
    //     response->body->len = 0x01;
    //     // user Id
    //     response->body->value = 0x01;
    //     free((void *)password);
    //     dbm_close(dbo.db);
    // return 0;

    // error:
    //     response->body->tag   = INTEGER;
    //     response->body->len   = 0x01;
    //     response->body->value = (uint8_t)*response->code;
    //     dbm_close(dbo.db);
    //     return -1;
}

// ssize_t account_create(const request_t *request)
// {
//     char    db_name[] = "mydb";
//     ssize_t result;
//     DBO     dbo;
//     char   *existing;

//     const acc_t *acc = (acc_t *)request->body;

//     printf("username: %s\n", acc->username);
//     printf("password: %s\n", acc->password);

//     dbo.name = db_name;

//     result = database_open(&dbo, err);
//     if(result == -1)
//     {
//         perror("database error");
//         *response->code = SERVER_ERROR;
//         goto error;
//     }

//     // check user exists
//     existing = retrieve_string(dbo.db, (char *)acc->username);

//     if(existing)
//     {
//         printf("Retrieved username: %s\n", existing);
//         *response->code = USER_EXISTS;
//         free((void *)existing);
//         goto error;
//     }

//     // Store user
//     if(store_string(dbo.db, (char *)acc->username, (char *)acc->password) != 0)
//     {
//         perror("user");
//         *response->code = SERVER_ERROR;
//         goto error;
//     }

//     response->body->tag   = ACC_Login;
//     response->body->len   = 0x01;
//     response->body->value = ACC_Create;
//     dbm_close(dbo.db);
//     return 0;

// error:
//     response->body->tag   = INTEGER;
//     response->body->len   = 0x01;
//     response->body->value = (uint8_t)*response->code;
//     dbm_close(dbo.db);
//     return -1;
// }

ssize_t account_logout(const request_t *request)
{
    printf("in account_logout %d \n", *request->client_fd);
    return 0;
}
