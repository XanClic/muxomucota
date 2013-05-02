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

#include <ipc.h>
#include <stdint.h>
#include <stdio.h>
#include <system-timer.h>
#include <unistd.h>


#define THREADS 100000
#define TIMEOUT 1000


static volatile int got = 0;
static volatile int last_popup, first_popup = 0;

static uintmax_t handler(void)
{
    int elapsed = elapsed_ms();

    __sync_bool_compare_and_swap(&first_popup, 0, elapsed);
    last_popup = elapsed;

    got++;

    return 0;
}


int main(void)
{
    popup_ping_handler(0, handler);

    pid_t pid = getpid();

    int spawn_start = elapsed_ms();

    printf("Spawning threads for PID %i...\n", pid);

    for (int i = 0; i < THREADS; i++)
    {
        errno = 0;
        ipc_ping(pid, 0);

        if (errno == EAGAIN)
        {
            yield();
            i--;
        }
        else if (errno)
            perror("ipc_ping");
    }

    int retire_start = elapsed_ms();

    printf("Done (in %i ms)\n", retire_start - spawn_start);

    printf("Waiting for retirement...\n");

    int timeout = elapsed_ms() + TIMEOUT;

    while ((got < THREADS) && (elapsed_ms() < timeout))
        yield();

    if (got == THREADS)
        printf("Successful (%i ms total, %i ms since last ping).\n", last_popup - first_popup, elapsed_ms() - retire_start);
    else
        printf("Timeout, %i of %i returned.\n", got, THREADS);

    return 0;
}
