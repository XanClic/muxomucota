#include <stdarg.h>
#include <unistd.h>


int execlp(const char *file, const char *arg, ...)
{
    va_list va1, va2;

    va_start(va1, arg);
    va_copy(va2, va1);

    int arg_count = 1;

    while (va_arg(va1, const char *) != NULL)
        arg_count++;

    va_end(va1);


    const char *argv[arg_count + 1];

    argv[0] = arg;

    for (int i = 1; argv[i - 1] != NULL; i++)
        argv[i] = va_arg(va2, const char *);

    va_end(va2);


    return execvp(file, (char *const *)argv);
}
