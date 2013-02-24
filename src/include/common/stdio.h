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


extern FILE *stdin, *stdout, *stderr;


int getchar(void);
int putchar(int c);

int puts(const char *s);

int printf(const char *s, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *fp, const char *s, ...) __attribute__((format(printf, 2, 3)));
int sprintf(char *buf, const char *s, ...) __attribute__((format(printf, 2, 3)));
int snprintf(char *str, size_t n, const char *format, ...) __attribute__((format(printf, 3, 4)));
int asprintf(char **strp, const char *format, ...) __attribute__((format(printf, 2, 3)));
int vfprintf(FILE *fp, const char *format, va_list args);
int vprintf(const char *format, va_list args);
int vsprintf(char *str, const char *format, va_list args);
int vsnprintf(char *buffer, size_t n, const char *format, va_list argptr);

void perror(const char *s);

int fflush(FILE *fp);


static inline int filepipe(FILE *fp) { return fp->fd; }

#endif
