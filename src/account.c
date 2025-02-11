#include "account.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <string.h>

ssize_t packet_handler(const request_t *request, response_t *response)
{
    ssize_t result;

    result = -1;
    printf("packet_handler: header->type: %d\n", (int)request->header->type);

    if(request->header->type == ACC_Login)
    {
        result = account_login(request, response);
    }

    return result;
}

static ssize_t auth(body_t *body, res_body_t *res_body)
{
    const char username[] = "Testing";
    const char password[] = "Password123";

    const acc_t *acc = (acc_t *)body;

    printf("username: %s\n", acc->username);
    printf("password: %s\n", acc->password);

    if(strcmp(username, (char *)acc->username) != 0 || strcmp(password, (char *)acc->password) != 0)
    {
        res_body->tag = INTEGER;
        res_body->len = 0x01;
        // user Id
        res_body->value = INVALID_AUTH;
        return -1;
    }
    res_body->tag = INTEGER;
    res_body->len = 0x01;
    // user Id
    res_body->value = 0x01;

    return 0;
}

ssize_t account_login(const request_t *request, response_t *response)
{
    printf("account_login\n");

    if(auth(request->body, response->body) < 0)
    {
        printf("invalid auth\n");
        *(response->code) = INVALID_AUTH;
    }
    return 0;
}
