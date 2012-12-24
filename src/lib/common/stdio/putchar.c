#include <stdio.h>
#include <unistd.h>
#include <vfs.h>


int putchar(int c)
{
    return stream_send(STDOUT_FILENO, &c, 1, 0) ? (int)(unsigned char)c : EOF;
}
