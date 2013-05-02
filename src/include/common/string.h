/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#ifndef _STRING_H
#define _STRING_H

#include <macro-helpers.h>
#include <stddef.h>
#include <stdint.h>


#define STRTOK_FOREACH(string, delim, target) \
    for (char *__lu(saveptr), *target = strtok_r(string, delim, &__lu(saveptr)); target != NULL; target = strtok_r(NULL, delim, &__lu(saveptr)))


#if __STDC_VERSION__ >= 199901L
void *memcpy(void *restrict d, const void *restrict s, size_t n);
char *strcat(char *restrict d, const char *restrict s);
char *strcpy(char *restrict d, const char *restrict s);
char *strncat(char *restrict d, const char *restrict s, size_t n);
char *strncpy(char *restrict d, const char *restrict s, size_t n);
#else
void *memcpy(void *d, const void *s, size_t n);
char *strcat(char *d, const char *s);
char *strcpy(char *d, const char *s);
char *strncat(char *d, const char *s, size_t n);
char *strncpy(char *d, const char *s, size_t n);
#endif

static inline int ffsl(long i) { return __builtin_ffsl(i); }

void *memchr(const void *s, int c, size_t n);
int memcmp(const void *m1, const void *m2, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memrchr(const void *s, int c, size_t n);
void *memset(void *s, int c, size_t n);
void *memsetptr(void *s, uintptr_t c, size_t n);
int strcasecmp(const char *s1, const char *s2);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strcoll(const char *s1, const char *s2);
int strcspn(const char *s1, const char *s2);
char *strdup(const char *s);
size_t strlen(const char *s);
int strlencmp(const char *s1, const char *s2, size_t s1_len);
int strncmp(const char *s1, const char *s2, size_t n);
char *strpbrk(const char *s1, const char *s2);
size_t strnlen(const char *s, size_t mlen);
char *strrchr(const char *s, int c);
int strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);
char *strerror(int errnum);

int strtok_count(char *str, const char *delim);
void strtok_array(char **array, char *str, const char *delim);

#endif
