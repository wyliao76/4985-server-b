#include "messaging.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR_READ (-5)

ssize_t request_handler(int connfd)
{
    ssize_t retval;
    int     err;

    request_t  request;
    header_t   req_header;
    response_t response;
    header_t   res_header;
    uint8_t   *buf;
    code_t     code;
    size_t     response_len;
    uint8_t   *res_buf;

    err                          = 0;
    code                         = OK;
    request.header               = &req_header;
    request.header_len           = sizeof(header_t);
    response.header              = &res_header;
    response.header->payload_len = 0;
    response.code                = &code;
    response_len                 = 0;

    request.body = (body_t *)malloc(sizeof(body_t));
    if(!request.body)
    {
        perror("Failed to allocate request body");
        retval = -1;
        goto done;
    }

    response.body = (body_t *)malloc(sizeof(body_t));
    if(!response.body)
    {
        perror("Failed to allocate response body");
        retval = -1;
        goto error1;
    }

    buf = (uint8_t *)calloc(request.header_len, sizeof(uint8_t));
    if(!buf)
    {
        perror("Failed to allocate buf");
        retval = -1;
        goto error2;
    }

    if(read_packet(connfd, &buf, &request, &response, &err) < 0)
    {
        errno = err;
        perror("request_handler::read_fd_until_eof");
        retval = -1;
        goto error3;
    }

    if(create_response(&request, &response, &res_buf, &response_len, &err) < 0)
    {
        errno = err;
        perror("request_handler::create_response");
        retval = -1;
        goto error4;
    }

    sent_response(connfd, res_buf, &response_len);

    retval = EXIT_SUCCESS;

error4:
    free(res_buf);
error3:
    free(buf);
error2:
    free(response.body);
error1:
    free(request.body);
done:
    return retval;
}

ssize_t read_packet(int fd, uint8_t **buf, request_t *request, response_t *response, int *err)
{
    size_t header_len = request->header_len;

    uint16_t payload_len;
    ssize_t  nread;

    // Read header_len bytes from fd
    errno = 0;
    nread = read(fd, *buf, header_len);
    if(nread < 0)
    {
        *err = errno;
        perror("read_packet::read");
        *(response->code) = SERVER_ERROR;
        return -1;
    }

    // Deserialize header to reload payload length
    if(deserialize_header(request->header, response, *buf, nread) < 0)
    {
        // If what was read is lower than expected, the header is incomplete
        fprintf(stderr, "read_packet::nread<header_len: Message read was too short to be a header.\n");
        return -2;
    }

    // Pull the length of the payload
    payload_len = request->header->payload_len; /* header->size */

    // Allocate another payload_length bytes to buffer
    {
        uint8_t *newbuf;

        newbuf = (uint8_t *)realloc(*buf, header_len + payload_len);
        if(newbuf == NULL)
        {
            perror("read_packet::realloc");
            *(response->code) = SERVER_ERROR;
            return -4;
        }
        *buf = newbuf;
    }

    // Read payload_length into buffer
    errno = 0;
    nread = read(fd, *buf + header_len, payload_len);
    if(nread < 0)
    {
        *err = errno;
        perror("read_packet::read");
        *(response->code) = SERVER_ERROR;
        return ERR_READ;
    }

    if(deserialize_body(request, response, *buf + header_len, nread, err) < 0)
    {
        // If what was read is lower than expected, the header is incomplete
        fprintf(stderr, "read_packet::body length mismatch\n");
        return -2;
    }

    return 0;
}

ssize_t deserialize_header(header_t *header, response_t *response, const uint8_t *buf, ssize_t nread)
{
    size_t offset;

    offset = 0;

    if(nread < (ssize_t)sizeof(header_t))
    {
        *(response->code) = INVALID_REQUEST;
        return -1;
    }

    memcpy(&header->type, buf + offset, sizeof(header->type));
    offset += sizeof(header->type);

    memcpy(&header->version, buf + offset, sizeof(header->version));
    offset += sizeof(header->version);

    memcpy(&header->sender_id, buf + offset, sizeof(header->sender_id));
    offset += sizeof(header->sender_id);
    header->sender_id = ntohs(header->sender_id);

    memcpy(&header->payload_len, buf + offset, sizeof(header->payload_len));
    header->payload_len = ntohs(header->payload_len);

    printf("header->type: %d\n", (int)header->type);
    printf("header->version: %d\n", (int)header->version);
    printf("header->sender_id: %d\n", (int)header->sender_id);
    printf("header->payload_len: %d\n", (int)header->payload_len);

    return 0;
}

ssize_t deserialize_body(request_t *request, response_t *response, const uint8_t *buf, ssize_t nread, int *err)
{
    if(nread < (ssize_t)request->header->payload_len)
    {
        printf("body too short\n");
        *err              = EINVAL;
        *(response->code) = INVALID_REQUEST;
        return -1;
    }

    printf("type: %d\n", (int)request->header->type);

    if(request->header->type == ACC_Create || request->header->type == ACC_Login)
    {
        size_t offset;
        acc_t *acc;

        acc = (acc_t *)malloc(sizeof(acc_t));
        if(!acc)
        {
            perror("Failed to allocate acc_t");
            *err              = errno;
            *(response->code) = SERVER_ERROR;
            return -1;
        }
        memset(acc, 0, sizeof(acc_t));

        offset = 0;
        // Deserialize username tag
        memcpy(&acc->username_tag, buf + offset, sizeof(acc->username_tag));
        offset += sizeof(acc->username_tag);

        printf("tag: %d\n", (int)acc->username_tag);

        // Deserialize username length
        memcpy(&acc->username_len, buf + offset, sizeof(acc->username_len));
        offset += sizeof(acc->username_len);

        printf("len: %d\n", (int)acc->username_len);

        // Deserialize username
        acc->username = (uint8_t *)malloc((size_t)acc->username_len + 1);
        if(!acc->username)
        {
            perror("Failed to allocate username");
            free(acc);
            *err              = errno;
            *(response->code) = SERVER_ERROR;
            return -1;
        }
        memcpy(acc->username, buf + offset, acc->username_len);
        acc->username[acc->username_len] = '\0';
        offset += acc->username_len;

        printf("username: %s\n", acc->username);

        // Deserialize username tag
        memcpy(&acc->password_tag, buf + offset, sizeof(acc->password_tag));
        offset += sizeof(acc->password_tag);

        printf("password_tag: %d\n", (int)acc->password_tag);

        // Deserialize password length
        memcpy(&acc->password_len, buf + offset, sizeof(acc->password_len));
        offset += sizeof(acc->password_len);

        printf("password_len: %d\n", (int)acc->password_len);

        // Deserialize password
        acc->password = (uint8_t *)malloc((size_t)acc->password_len + 1);
        if(!acc->password)
        {
            perror("Failed to allocate password");
            free(acc->username);
            free(acc);
            *err              = errno;
            *(response->code) = SERVER_ERROR;
            return -1;
        }
        memcpy(acc->password, buf + offset, acc->password_len);
        acc->password[acc->password_len] = '\0';

        free(request->body);
        request->body = (body_t *)acc;

        printf("password: %s\n", acc->password);

        return 0;
    }

    printf("Unknown type: 0x%X\n", request->header->type);
    *(response->code) = INVALID_REQUEST;
    return -1;
}

ssize_t serialize_header(const response_t *response, uint8_t *buf)
{
    size_t offset;

    offset = 0;

    memcpy(buf + offset, &response->header->type, sizeof(response->header->type));
    offset += sizeof(response->header->type);

    memcpy(buf + offset, &response->header->version, sizeof(response->header->version));
    offset += sizeof(response->header->version);

    response->header->sender_id = htons(response->header->sender_id);
    memcpy(buf + offset, &response->header->sender_id, sizeof(response->header->sender_id));
    offset += sizeof(response->header->sender_id);

    response->header->payload_len = htons(response->header->payload_len);
    memcpy(buf + offset, &response->header->payload_len, sizeof(response->header->payload_len));

    return 0;
}

ssize_t serialize_body(const response_t *response, uint8_t *buf)
{
    printf("yo: %d\n", (int)htons(response->header->payload_len));
    memcpy(buf, response->body->msg, htons(response->header->payload_len));

    return 0;
}

ssize_t create_response(const request_t *request, response_t *response, uint8_t **buf, size_t *response_len, int *err)
{
    const u_int8_t msg[3] = {
        0x02,
        0x01,
        0x01,
    };

    response->header->type        = ACC_Login_Success;
    response->header->version     = 1;
    response->header->sender_id   = 0;
    response->header->payload_len = 3;

    *response_len = (size_t)(request->header_len + response->header->payload_len);
    printf("response_len: %d\n", (int)*response_len);

    *buf = (uint8_t *)malloc(*response_len);
    if(*buf == NULL)
    {
        perror("read_packet::realloc");
        *(response->code) = SERVER_ERROR;
        *err              = errno;
        return -1;
    }

    response->body->msg = (uint8_t *)malloc(response->header->payload_len);
    if(!response->body->msg)
    {
        perror("Failed to allocate memory for msg");
        *err = errno;
        return -1;
    }

    memcpy(response->body->msg, msg, response->header->payload_len);

    serialize_header(response, *buf);
    serialize_body(response, *buf + sizeof(header_t));

    free(response->body->msg);

    return 0;
}

ssize_t sent_response(int fd, const uint8_t *buf, const size_t *response_len)
{
    ssize_t result;

    result = write(fd, buf, *response_len);
    printf("\n%d\n", (int)result);
    return 0;
}
