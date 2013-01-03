#include <stdio.h>
#include <vfs.h>


int fflush(FILE *fp)
{
    pipe_set_flag(filepipe(fp), F_FLUSH, 1);
    return 0;
}
