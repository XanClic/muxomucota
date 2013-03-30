#include <stdio.h>
#include <unistd.h>
#include <vfs.h>


int fputc(int c, FILE *fp)
{
    return stream_send(filepipe(fp), &c, 1, 0) ? (int)(unsigned char)c : EOF;
}
