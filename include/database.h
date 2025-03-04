// cppcheck-suppress-file unusedStructMember

#ifndef DATABASE_H
#define DATABASE_H

#include <ndbm.h>
#include <sys/types.h>

#ifdef __APPLE__
typedef size_t datum_size;
#else
typedef int datum_size;
#endif

#define TO_SIZE_T(x) ((size_t)(x))

typedef struct
{
    const void *dptr;
    datum_size  dsize;
} const_datum;

#define MAKE_CONST_DATUM(str) ((const_datum){(str), (datum_size)strlen(str) + 1})

#define MAKE_CONST_DATUM_BYTE(str, size) ((const_datum){(str), (datum_size)(size)})

typedef struct DBO
{
    char *name;
    DBM  *db;
} DBO;

ssize_t database_open(DBO *dbo, int *err);

int store_string(DBM *db, const char *key, const char *value);

int store_int(DBM *db, const char *key, int value);

int store_byte(DBM *db, const char *key, size_t k_size, const char *value, size_t v_size);

char *retrieve_string(DBM *db, const char *key);

int retrieve_int(DBM *db, const char *key, int *result);

void *retrieve_byte(DBM *db, const void *key, size_t size);

#endif    // DATABASE_H
