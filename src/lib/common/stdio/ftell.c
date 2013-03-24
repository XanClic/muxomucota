#include <stdio.h>
#include <vfs.h>

long ftell(FILE *stream)
{
    return pipe_get_flag(filepipe(stream), F_POSITION);
}
