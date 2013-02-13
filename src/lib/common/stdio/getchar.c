#include <stdio.h>
#include <unistd.h>
#include <vfs.h>


int getchar(void)
{
    char c;
    return stream_recv(STDIN_FILENO, &c, 1, 0) ? c : EOF;
}
