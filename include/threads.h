// cppcheck-suppress-file unusedStructMember

#ifndef THREADS_H
#define THREADS_H

#include <stdlib.h>

#define O_THREAD_JOIN 1

typedef struct
{
    int connfd;
} thread_args;

int   start_thread(void *(*thread_fn)(void *targs), void *targs, size_t opts);

#endif
