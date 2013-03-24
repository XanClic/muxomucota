#include <stdio.h>

void clearerr(FILE *stream)
{
    stream->eof = false;
}
