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

    request_t request;
    header_t  header;
    uint8_t  *buf;

    err            = 0;
    request.header = &header;

    request.body = (body_t *)malloc(sizeof(body_t));
    if(!request.body)
    {
        perror("Failed to allocate body");
        return -1;
    }

    if(read_packet(connfd, &buf, &request, &err) < 0)
    {
        errno = err;
        perror("request_handler::read_fd_until_eof");
    }

    free(buf);
    free(request.body);

    retval = EXIT_SUCCESS;
    return retval;
}

ssize_t read_packet(int fd, uint8_t **buf, request_t *request, int *err)
{
    size_t header_len = sizeof(header_t);

    uint16_t payload_len;
    ssize_t  nread;

    *buf = (uint8_t *)calloc(header_len, sizeof(uint8_t));
    if(*buf == NULL)
    {
        return -1;
    }

    // Read header_len bytes from fd
    errno = 0;
    nread = read(fd, *buf, header_len);
    if(nread < 0)
    {
        *err = errno;
        perror("read_packet::read");
        return -1;
    }

    // Deserialize header to reload payload length
    if(deserialize_header(request->header, *buf, nread) < 0)
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
        return ERR_READ;
    }

    if(deserialize_body(request, *buf + header_len, nread, err) < 0)
    {
        // If what was read is lower than expected, the header is incomplete
        fprintf(stderr, "read_packet::body length mismatch\n");
        return -2;
    }

    return 0;

    // return (ssize_t)header_len + payload_len;
}

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */
ssize_t deserialize_header(header_t *header, const uint8_t *buf, ssize_t nread)
{
    size_t offset;

    offset = 0;

    if(nread < (ssize_t)sizeof(header_t))
    {
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

ssize_t deserialize_body(request_t *request, const uint8_t *buf, ssize_t nread, int *err)
{
    if(nread < (ssize_t)request->header->payload_len)
    {
        printf("body too short\n");
        *err = EINVAL;
        return -1;
    }

    printf("type: %d\n", (int)request->header->type);

    if(request->header->type == ACC_CREATE || request->header->type == ACC_LOGIN)
    {
        size_t offset;
        acc_t *acc;

        acc = (acc_t *)malloc(sizeof(acc_t));
        if(!acc)
        {
            perror("Failed to allocate acc_t");
            *err = errno;
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
            *err = errno;
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
            *err = errno;
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
    return -1;
}
