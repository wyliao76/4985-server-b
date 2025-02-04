#ifndef DATABASE_H
#define DATABASE_H

#include <ndbm.h>
#include <sys/types.h>

typedef struct DBO
{
    // cppcheck-suppress unusedStructMember
    const char *name;
    // cppcheck-suppress unusedStructMember
    DBM *db;
} DBO;

typedef struct ITEM
{
    // cppcheck-suppress unusedStructMember
    datum key;
    // cppcheck-suppress unusedStructMember
    datum value;
} ITEM;

ssize_t database_open(DBO *dbo, int *err);

ssize_t parse_datum(datum *obj, const char *input, size_t size, int *err);

ssize_t database_store(DBO *dbo, const char *key, const char *value, int *err);

ssize_t database_fetch(DBO *dbo, const char *key, datum *output, int *err);

ssize_t database_fetch_all(DBM *db, int *err);

#endif    // DATABASE_H
