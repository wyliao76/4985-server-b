#include "database.h"
#include <stdio.h>

int main(void)
{
    ssize_t result;
    int     err;

    result = database_connect(&err);
    printf("result: %d\n", (int)result);

    if(result == -1)
    {
        perror("database error");
    }
}
