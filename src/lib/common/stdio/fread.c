#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <vfs.h>

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t addend = 0;

    if (stream->bufsz)
    {
        char *cbuf = ptr;

        while ((stream->bufsz >= size) && nmemb)
        {
            for (unsigned i = 0; i < size; i++)
                *(cbuf++) = stream->buffer[--stream->bufsz];
            nmemb--;
            addend++;
        }

        if (!nmemb)
            return addend;

        // TODO: Was, wenn stream->bufsz != 0?
    }

    size_t got = stream_recv(filepipe(stream), ptr, size * nmemb, 0) / size;
    if (got + addend < nmemb)
        stream->eof = true;
    return got + addend;
}
