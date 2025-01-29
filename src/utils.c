#include "utils.h"
#include <stdlib.h>

/* Calls `free()` and nullifies the ptr. */
void nfree(void **ptr)
{
    if(ptr != NULL && *ptr != NULL)
    {
        free(*ptr);
        *ptr = NULL;
    }
}
