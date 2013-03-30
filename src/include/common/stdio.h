#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <vfs.h>


#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

#define EOF (-1)

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif


typedef struct FILE
{
    int fd;
    char buffer[16];
    size_t bufsz;
    bool eof;
} FILE;


extern FILE *stdin, *stdout, *stderr;


int getchar(void);
int fgetc(FILE *fp);
#define getc(c, fp) fgetc(c, fp)
int putchar(int c);
int fputc(int c, FILE *fp);
#define putc(c, fp) fputc(c, fp)

int puts(const char *s);
int fputs(const char *s, FILE *fp);

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


FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fclose(FILE *stream);

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

int fseek(FILE *stream, long offset, int whence);
void rewind(FILE *stream);
long ftell(FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);


static inline int filepipe(FILE *fp) { return fp->fd; }

#endif
