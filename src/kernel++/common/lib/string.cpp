/************************************************************************
 * Copyright (C) 2013 by Max Reitz                                      *
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

#include <cstddef>
#include <cstdint>
#include <cstring>


void *memset(void *s, int c, size_t n)
{
    uint8_t *d = static_cast<uint8_t *>(s);

    while (n--) {
        *(d++) = c;
    }

    return s;
}


void *memsetptr(void *s, uintptr_t c, size_t n)
{
    uintptr_t *d = static_cast<uintptr_t *>(s);

    while (n--) {
        *(d++) = c;
    }

    return s;
}


void *memcpy(void *restrict d, const void *restrict s, size_t n)
{
    uint8_t *restrict dst = static_cast<uint8_t *>(d);
    const uint8_t *restrict src = static_cast<const uint8_t *>(s);

    while (n--) {
        *(dst++) = *(src++);
    }

    return d;
}


char *strcpy(char *restrict d, const char *restrict s)
{
    uint8_t *restrict dst = reinterpret_cast<uint8_t *>(d);
    const uint8_t *restrict src = reinterpret_cast<const uint8_t *>(s);

    do {
        *(dst++) = *src;
    } while (*(src++));

    return d;
}


char *strncpy(char *restrict d, const char *restrict s, size_t n)
{
    uint8_t *restrict dst = reinterpret_cast<uint8_t *>(d);
    const uint8_t *restrict src = reinterpret_cast<const uint8_t *>(s);

    while (n && *src) {
        *(dst++) = *(src++);
        n--;
    }

    while (n--) {
        *(dst++) = 0;
    }

    return d;
}


int strncmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while (n--) {
        if (*(s1++) != *s2) {
            return static_cast<unsigned char>(s2[0]) - static_cast<unsigned char>(s1[-1]);
        }

        if (!*(s2++)) {
            return 0;
        }
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
    static char *next_token = nullptr;
    char *tok;
    int slen = strlen(string2);

    if (!string1) {
        string1 = next_token;
    }

    if (!string1) {
        return nullptr;
    }

    while ((tok = strstr(string1, string2)) == string1) {
        string1 += slen;
    }

    if (!*string1) {
        next_token = nullptr;
        return nullptr;
    }

    if (tok) {
        *tok = '\0';
        next_token = tok + slen;
    } else {
        next_token = nullptr;
    }

    return string1;
}


char *strstr(const char *restrict s1, const char *restrict s2)
{
    int i;

    while (*s1) {
        for (i = 0; s2[i] && (s1[i] == s2[i]); i++);

        if (!s2[i]) {
            return const_cast<char *>(s1);
        }

        s1++;
    }

    return nullptr;
}


/*
char *strdup(const char *s)
{
    char *os = kmalloc(strlen(s) + 1);
    strcpy(os, s);
    return os;
}
*/


char *strchr(const char *s, int c)
{
    while ((*s != c) && *s) {
        s++;
    }

    if (*s) {
        return const_cast<char *>(s);
    } else {
        return nullptr;
    }
}


int strtok_count(char *str, const char *delim)
{
    int count = 0;

    for (int i = 0; str[i]; i++) {
        if (strchr(delim, str[i])) {
            count++;
        }
    }

    return count + 1;
}


void strtok_array(char **array, char *str, const char *delim)
{
    int i = 0;

    for (char *tok = strtok(str, delim); tok; tok = strtok(nullptr, delim)) {
        array[i++] = tok;
    }
}


int strcmp(const char *s1, const char *s2)
{
    int ret;

    while (!(ret = static_cast<unsigned char>(*(s2++)) - static_cast<unsigned char>(*s1))) {
        if (!*(s1++)) {
            return 0;
        }
    }

    return ret;
}


char *strcat(char *restrict d, const char *restrict s)
{
    char *vd = d;

    while (*(vd++));
    vd--;

    while (*s) {
        *(vd++) = *(s++);
    }
    *vd = 0;

    return d;
}
