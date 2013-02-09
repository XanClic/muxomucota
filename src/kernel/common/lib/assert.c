#include <kassert.h>
#include <panic.h>
#include <stdnoreturn.h>
#include <string.h>


noreturn void assertion_panic(const char *file, int line, const char *function, const char *condition)
{
    size_t buflen = strlen(file) + strlen(function) + strlen(condition) + 64;
    char buffer[buflen];

    int i = 11;
    char linestr[i];
    linestr[--i] = 0;

    while (line != 0)
    {
        linestr[--i] = line % 10 + '0';
        line /= 10;
    }

    strcpy(buffer, "Internal exception: assertion failed\n\n");
    strcat(buffer, file);
    strcat(buffer, ":");
    strcat(buffer, &linestr[i]);
    strcat(buffer, ": ");
    strcat(buffer, function);
    strcat(buffer, "():\n");
    strcat(buffer, condition);


    panic(buffer);
}
