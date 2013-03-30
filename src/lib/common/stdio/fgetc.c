#include <stdio.h>
#include <unistd.h>
#include <vfs.h>


int fgetc(FILE *fp)
{
    char c;
    return stream_recv(filepipe(fp), &c, 1, 0) ? c : EOF;
}
