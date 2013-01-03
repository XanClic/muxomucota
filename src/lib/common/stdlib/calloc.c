#include <stdlib.h>
#include <string.h>


void *calloc(size_t nmemb, size_t size)
{
    void *mem = malloc(nmemb * size);
    memset(mem, 0, nmemb * size);
    return mem;
}
