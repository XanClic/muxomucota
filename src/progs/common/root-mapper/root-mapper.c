#include <drivers.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


char *basepath;


bool service_is_symlink(const char *relpath, char *linkpath)
{
    strcpy(linkpath, basepath);
    strcat(linkpath, relpath);

    return true;
}


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: root-mapper <base path>\n");
        return 1;
    }

    basepath = argv[1];

    daemonize("root");
}
