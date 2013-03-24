#include <stdio.h>
#include <vfs.h>

void rewind(FILE *stream)
{
    pipe_set_flag(filepipe(stream), F_POSITION, 0);
}
