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

#include <kmalloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


void *memset(void *s, int c, size_t n)
{
    uint8_t *d = s;

    while (n--)
        *(d++) = c;

    return s;
}


void *memsetptr(void *s, uintptr_t c, size_t n)
{
    uintptr_t *d = s;

    while (n--)
        *(d++) = c;

    return s;
}


void *memcpy(void *restrict d, const void *restrict s, size_t n)
{
    uint8_t *restrict dst = d;
    const uint8_t *restrict src = s;

    while (n--)
        *(dst++) = *(src++);

    return d;
}


char *strcpy(char *restrict d, const char *restrict s)
{
    uint8_t *restrict dst = (uint8_t *)d;
    const uint8_t *restrict src = (const uint8_t *)s;

    do
        *(dst++) = *src;
    while (*(src++));

    return d;
}


char *strncpy(char *restrict d, const char *restrict s, size_t n)
{
    uint8_t *restrict dst = (uint8_t *)d;
    const uint8_t *restrict src = (const uint8_t *)s;

    while (n && *src)
    {
        *(dst++) = *(src++);
        n--;
    }

    while (n--)
        *(dst++) = 0;

    return d;
}


int strncmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while (n--)
    {
        if (*(s1++) != *s2)
            return (unsigned char)s2[0] - (unsigned char)s1[-1];

        if (!*(s2++))
            return 0;
    }

    return 0;
}


size_t strlen(const char *s)
{
    size_t l;

    for (l = 0; s[l]; l++);

    return l;
}


char *strtok(char *restrict string1, const char *restrict string2)
{
    static char *next_token = NULL;
    char *tok;
    int slen = strlen(string2);

    if (string1 == NULL)
        string1 = next_token;

    if (string1 == NULL)
        return NULL;

    while ((tok = strstr(string1, string2)) == string1)
        string1 += slen;

    if (!*string1)
    {
        next_token = NULL;
        return NULL;
    }

    if (tok == NULL)
        next_token = NULL;
    else
    {
        *tok = '\0';
        next_token = tok + slen;
    }

    return string1;
}


char *strstr(const char *restrict s1, const char *restrict s2)
{
    int i;

    while (*s1)
    {
        for (i = 0; s2[i] && (s1[i] == s2[i]); i++);

        if (!s2[i])
            return (char *)s1;

        s1++;
    }

    return NULL;
}


char *strdup(const char *s)
{
    char *os = kmalloc(strlen(s) + 1);
    strcpy(os, s);
    return os;
}


char *strchr(const char *s, int c)
{
    while ((*s != c) && *s)
        s++;

    if (!*s)
        return NULL;
    return (char *)s;
}


int strtok_count(char *str, const char *delim)
{
    int count = 0;

    for (int i = 0; str[i]; i++)
        if (strchr(delim, str[i]) != NULL)
            count++;

    return count + 1;
}


void strtok_array(char **array, char *str, const char *delim)
{
    int i = 0;

    for (char *tok = strtok(str, delim); tok != NULL; tok = strtok(NULL, delim))
        array[i++] = tok;
}


int strcmp(const char *s1, const char *s2)
{
    int ret;

    while (!(ret = (unsigned char)*(s2++) - (unsigned char)*s1))
        if (!*(s1++))
            return 0;

    return ret;
}


char *strcat(char *restrict d, const char *restrict s)
{
    char *vd = d;

    while (*(vd++));
    vd--;

    while (*s)
        *(vd++) = *(s++);
    *vd = 0;

    return d;
}
