#ifndef NETTOOLS_H
#define NETTOOLS_H

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn static inline void version(char *argv0, int exitcode)
{
    printf("%s is part of the paloxena3.5 nettools.\n", basename(argv0));
    printf("Copyright (c) 2010, 2013 Hanna Reitz.\n");
    printf("License: MIT <http://www.opensource.org/licenses/mit-license.php>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");

    exit(exitcode);
}

#endif
