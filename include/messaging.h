// cppcheck-suppress-file unusedStructMember

#ifndef MESSAGING_H
#define MESSAGING_H

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */
typedef struct
{
    uint8_t  type;
    uint8_t  version;
    uint16_t sender_id;
    uint16_t payload_len;
} header_t;

/* TODO: THESE SHOULD NOT BE HERE, ONLY FOR DEMO */

int deserialize_header(header_t *header, const uint8_t *buf, ssize_t nread);

ssize_t read_packet(int fd, uint8_t **buf, header_t *header, int *err);

#endif
