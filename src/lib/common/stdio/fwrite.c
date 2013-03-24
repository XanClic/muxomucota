#include <stddef.h>
#include <stdio.h>
#include <vfs.h>

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return stream_send(filepipe(stream), ptr, size * nmemb, 0) / size;
}
