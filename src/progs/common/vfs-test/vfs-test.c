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

#include <assert.h>
#include <shm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system-timer.h>
#include <unistd.h>
#include <vfs.h>


extern void _provide_vfs_services(void);


static big_size_t expected_size;
static void *expected_data;


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    if (id > 2)
    {
        fprintf(stderr, "Invalid ID\n");
        return 0;
    }

    assert((uintptr_t)data > 1048576);

    if (id == 2)
        return size;

    if (size != expected_size)
    {
        fprintf(stderr, "Expected %llu bytes, got %llu\n", expected_size, size);
        return 0;
    }

    if (memcmp(data, expected_data, size))
    {
        fprintf(stderr, "Memory difference\n");
        return 0;
    }

    return size;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    if (id > 2)
    {
        fprintf(stderr, "Invalid ID\n");
        return 0;
    }

    assert((uintptr_t)data > 1048576);

    if (id == 2)
        return size;

    if (size != expected_size)
    {
        fprintf(stderr, "Expected %llu bytes, got %llu\n", expected_size, size);
        return 0;
    }

    memcpy(data, expected_data, size);

    return size;
}


int main(void)
{
    _provide_vfs_services();

    int fd = 3;

    _pipes[fd].pid = getpid();
    _pipes[fd].id  = 1;

    srand(elapsed_ms());

    printf("=== Unit test ===\nstream_send:\n");

    for (int i = (1 << 29); i >= 1; i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        expected_size = i;
        expected_data = malloc(i);

        for (int j = 0; j < i; j += 2)
            ((uint16_t *)expected_data)[j / 2] = rand() & 0xffff;

        int t = elapsed_ms();

        big_size_t trans_size = stream_send(fd, expected_data, expected_size, 0);

        t = elapsed_ms() - t;

        if (trans_size != expected_size)
            fprintf(stderr, "Bad return value, expected %llu, got %llu\n", expected_size, trans_size);

        printf("[%i] ", t);
        fflush(stdout);

        free(expected_data);
    }

    printf("\nstream_recv:\n");

    for (int i = (1 << 29); i >= 1; i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        expected_size = i;
        expected_data = malloc(i);

        void *recv = malloc(i);

        for (int j = 0; j < i; j += 2)
            ((uint16_t *)expected_data)[j / 2] = rand() & 0xffff;

        int t = elapsed_ms();

        big_size_t trans_size = stream_recv(fd, recv, expected_size, 0);

        t = elapsed_ms() - t;

        if (trans_size != expected_size)
            fprintf(stderr, "Bad return value, expected %llu, got %llu\n", expected_size, trans_size);

        if ((expected_data != NULL) && memcmp(recv, expected_data, expected_size))
            fprintf(stderr, "Memory difference\n");

        printf("[%i] ", t);
        fflush(stdout);

        free(expected_data);
        free(recv);
    }

    _pipes[fd].id  = 2;

    printf("\n=== Speed test (standard) ===\nstream_send:\n");

    void *buf = malloc(1 << 29);

    for (int i = (1 << 29); i >= (1 << 10); i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        int t = elapsed_ms();
        stream_send(fd, buf, i, 0);
        t = elapsed_ms() - t;

        printf("[%i] ", t);
        fflush(stdout);
    }

    printf("\nstream_recv:\n");

    for (int i = (1 << 29); i >= (1 << 10); i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        int t = elapsed_ms();
        stream_recv(fd, buf, i, 0);
        t = elapsed_ms() - t;

        printf("[%i] ", t);
        fflush(stdout);
    }

    free(buf);

    printf("\n=== Speed test (pure SHM) ===\nstream_send:\n");

    for (int i = (1 << 29); i >= (4 << 10); i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        uintptr_t shmid = shm_create(i);

        int t = elapsed_ms();
        stream_send_shm(fd, shmid, i, 0);
        t = elapsed_ms() - t;

        printf("[%i] ", t);
        fflush(stdout);
    }

    printf("\nstream_recv:\n");

    for (int i = (1 << 29); i >= (4 << 10); i /= 2)
    {
        if (i >> 20)
            printf("%iM", i >> 20);
        else if (i >> 10)
            printf("%ik", i >> 10);
        else
            printf("%i", i);
        fflush(stdout);

        uintptr_t shmid = shm_create(i);

        int t = elapsed_ms();
        stream_recv_shm(fd, shmid, i, 0);
        t = elapsed_ms() - t;

        printf("[%i] ", t);
        fflush(stdout);
    }

    putchar('\n');

    _pipes[fd].pid = 0;
    _pipes[fd].id  = 0;

    return 0;
}
