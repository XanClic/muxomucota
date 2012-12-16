#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#define MB_CUR_MAX 4

void *sbrk(intptr_t diff);

int clearenv(void);
char *getenv(const char *name);
int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

void *malloc(size_t sz);
void *calloc(size_t nmemb, size_t sz);
void *realloc(void *ptr, size_t size);
void free(void *mem);

noreturn void exit(int status);
noreturn void abort(void);

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);
long long atoq(const char *nptr);
long long strtoll(const char *s, char **endptr, int base);
long strtol(const char *s, char **endptr, int base);
long double strtold(const char *nptr, char **endptr);

int mbtowc(wchar_t *pwc, const char *s, size_t n);
int mblen(const char *s, size_t n);
int wctomb(char *s, wchar_t wc);
size_t wcstombs(char* buf, const wchar_t* wcs, size_t len);
size_t mbstowcs(wchar_t* buf, const char* str, size_t len);

int system(const char *c);

#endif
