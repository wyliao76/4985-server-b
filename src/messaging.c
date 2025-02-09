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

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */
int deserialize_header(header_t *header, const uint8_t *buf, ssize_t nread)
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

    printf("%d\n", header->type);
    printf("%d\n", header->version);
    printf("%d\n", header->sender_id);
    printf("%d\n", header->payload_len);

    return 0;
}

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */

ssize_t read_packet(int fd, uint8_t **buf, header_t *header, int *err)
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
        nfree((void **)&header);
        return -1;
    }

    // // If what was read is lower than expected, the header is incomplete
    // if(nread < (ssize_t)header_len)
    // {
    //     fprintf(stderr, "read_packet::nread<header_len: Message read was too short to be a header.\n");
    //     nfree((void **)&header);
    //     return -2;
    // }

    // Deserialize header to reload payload length
    if(deserialize_header(header, *buf, nread) < 0)
    {
        fprintf(stderr, "read_packet::deserialize_header: Failed to deserialize header.\n");
        return -3;
    }

    // Pull the length of the payload
    payload_len = header->payload_len; /* header->size */

    // Allocate another payload_length bytes to buffer
    {
        uint8_t *newbuf;

        newbuf = (uint8_t *)realloc(*buf, header_len + payload_len);
        if(newbuf == NULL)
        {
            perror("read_packet::realloc");
            nfree((void **)&header);
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
        nfree((void **)&header);
        return ERR_READ;
    }

    return (ssize_t)header_len + payload_len;
}
