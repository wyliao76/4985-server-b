#include "threads.h"
#include "utils.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define BUFLEN 1024

int start_thread(void *(*thread_fn)(void *targs), void *targs, size_t opts)
{
    pthread_t thread;

    if(thread_fn == NULL)
    {
        return -1;
    }

    errno = pthread_create(&thread, NULL, thread_fn, targs);
    if(errno != 0)
    {
        perror("start_thread::pthread_create");
        return -2;
    }

    if(opts & O_THREAD_JOIN)
    {
        errno = pthread_join(thread, NULL);
        if(errno != 0)
        {
            perror("start_thread::pthread_join");
            return -3;
        }
    }

    return 0;
}

void *thread_echo(void *targs)
{
    const thread_args *args;
    int                connfd;

    uint8_t *buf;
    ssize_t  nread;
    ssize_t  nwrote;

    args   = (thread_args *)targs;
    connfd = args->connfd;

    errno = 0;
    buf   = (uint8_t *)malloc(BUFLEN);
    if(buf == NULL)
    {
        perror("thread_echo::buf::malloc");
        goto cleanup;
    }

    do
    {
        errno = 0;
        nread = read(connfd, buf, BUFLEN);
        if(nread < 0)
        {
            perror("thread_echo::read");
            goto cleanup;
        }

        nwrote = 0;
        do
        {
            ssize_t twrote;
            size_t  remaining;

            remaining = (size_t)(nread - nwrote);
            errno     = 0;
            twrote    = write(connfd, &buf[nwrote], remaining);
            if(twrote < 0)
            {
                perror("thread_echo::write");
                goto cleanup;
            }

            nwrote += twrote;
        } while(nwrote != nread);
    } while(nread != 0);

cleanup:
    nfree((void **)&buf);
    close(connfd);
    return NULL;
}
