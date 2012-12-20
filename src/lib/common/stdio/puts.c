#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>


int puts(const char *s)
{
    size_t len = strlen(s);

    if (stream_send(STDOUT_FILENO, s, len, 0) < len)
        return EOF;

    if (stream_send(STDOUT_FILENO, "\n", 1, 0) < 1)
        return EOF;

    return 0;
}
