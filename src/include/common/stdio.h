#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <vfs.h>


#define EOF (-1)


typedef struct FILE
{
    int fd;
    char buffer[16];
    size_t bufsz;
    bool eof;
} FILE;


int putchar(int c);

int puts(const char *s);

int printf(const char *s, ...);
int fprintf(FILE *fp, const char *s, ...);
int sprintf(char *buf, const char *s, ...);
int snprintf(char *str, size_t n, const char *format, ...);
int vfprintf(FILE *fp, const char *format, va_list args);
int vprintf(const char *format, va_list args);
int vsprintf(char *str, const char *format, va_list args);
int vsnprintf(char *buffer, size_t n, const char *format, va_list argptr);


static inline int filepipe(FILE *fp) { return fp->fd; }

#endif
