#include <stddef.h>
#include <string.h>
#include <unistd.h>


extern char *_cwd;


char *getcwd(char *buf, size_t size)
{
    return (size < strlen(_cwd) + 1) ? NULL : strcpy(buf, _cwd);
}
