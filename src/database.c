#include "../include/database.h"
#include <errno.h>
#include <p101_c/p101_stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#pragma GCC diagnostic ignored "-Waggregate-return"

ssize_t database_open(DBO *dbo, int *err)
{
    dbo->db = dbm_open(dbo->name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(!dbo->db)
    {
        perror("dbm_open failed");
        *err = errno;
        return -1;
    }
    // key = dbm_firstkey(db);
    return 0;
}

ssize_t parse_datum(datum *obj, const char *input, size_t size, int *err)
{
    obj->dptr = malloc(size * sizeof(char));
    if(obj->dptr == NULL)
    {
        perror("malloc error");
        *err = errno;
        return -1;
    }
    memcpy(obj->dptr, input, size);
    obj->dsize = size;
    return 0;
}

ssize_t database_store(DBO *dbo, const char *key, const char *value, int *err)
{
    ITEM item;

    memset(&item, 0, sizeof(ITEM));

    parse_datum(&item.key, key, strlen(key) + 1, err);
    parse_datum(&item.value, value, strlen(value) + 1, err);

    dbm_store(dbo->db, item.key, item.value, DBM_REPLACE);
    return 0;
}

ssize_t database_fetch(DBO *dbo, const char *key, datum *output, int *err)
{
    parse_datum(output, key, strlen(key) + 1, err);

    *output = dbm_fetch(dbo->db, *output);
    if(!output->dptr)
    {
        return -1;
    }
    printf("Fetched value: %s\n", (char *)output->dptr);
    return 0;
}
