#include "../include/database.h"
#include <errno.h>
#include <ndbm.h>
#include <p101_c/p101_stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#pragma GCC diagnostic ignored "-Waggregate-return"

ssize_t database_connect(int *err)
{
    DBM  *db;
    datum key;
    datum value;
    datum result;

    const char *name   = "name";
    const char *nvalue = "Tia";
    void       *ptr;

    ptr = malloc(strlen(name) + 1);
    if(ptr == NULL)
    {
        perror("malloc error");
        *err = errno;
        return -1;
    }
    key.dptr = ptr;

    ptr = malloc(strlen(nvalue) + 1);
    if(ptr == NULL)
    {
        perror("malloc error");
        *err = errno;
        free(key.dptr);
        return -1;
    }
    value.dptr = ptr;

    ptr = malloc(strlen(name) + strlen(nvalue) + 1);
    if(ptr == NULL)
    {
        perror("malloc error");
        *err = errno;
        free(key.dptr);
        free(value.dptr);
        return -1;
    }
    result.dptr = ptr;

    db = dbm_open("mydb", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(!db)
    {
        perror("dbm_open failed");
        *err = errno;
        free(key.dptr);
        free(value.dptr);
        free(ptr);
        return -1;
    }

    memcpy(key.dptr, name, strlen(name) + 1);
    key.dsize = strlen((char *)key.dptr) + 1;

    memcpy(value.dptr, nvalue, strlen(nvalue) + 1);
    value.dsize = strlen((char *)value.dptr) + 1;

    dbm_store(db, key, value, DBM_REPLACE);

    result = dbm_fetch(db, key);
    if(result.dptr)
    {
        printf("Fetched value: %s\n", (char *)result.dptr);
    }

    dbm_close(db);

    free(key.dptr);
    free(value.dptr);
    free(ptr);

    return 0;
}
