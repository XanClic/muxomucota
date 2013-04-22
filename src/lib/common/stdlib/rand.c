#include <stdint.h>
#include <stdlib.h>


#define N 624
#define M 397

///Code aus der deutschen Wikipedia und leicht verändert
///http://de.wikipedia.org/wiki/Mersenne-Twister
static uint32_t mersenne_twister(uint32_t r, uint32_t s)
{
    const uint32_t A[2] = {0, 0x9908b0df};
    const uint32_t HI = 0x80000000;
    const uint32_t LO = 0x7fffffff;
    int i, k;
    uint32_t h, e;

    static uint32_t y[N];
    static int index = N+1;

    if (index >= N)
    {
        if (index > N)
        {
            //Initialisiere y mit Pseudozufallszahlen
            for (i = 0; i < N; i++)
            {
                r = 509845221 * r + 3;
                s *= s + 1;
                y[i] = s + (r >> 10);
            }
        }
        for (k = 0; k < N - M; k++)
        {
            h = (y[k] & HI) | (y[k + 1] & LO);
            y[k] = y[k + M] ^ (h >> 1) ^ A[h & 1];
        }
        for (k = N - M; k < N - 1; k++)
        {
            h = (y[k] & HI) | (y[k + 1] & LO);
            y[k] = y[k + (M - N)] ^ (h >> 1) ^ A[h & 1];
        }
        h = (y[N - 1] & HI) | (y[0] & LO);
        y[N - 1] = y[M - 1] ^ (h >> 1) ^ A[h & 1];
        index = 0;
    }

    e = y[index++];
    //Tempering
    e ^= (e >> 11);
    e ^= (e << 7) & 0x9d2c5680;
    e ^= (e << 15) & 0xefc60000;
    e ^= (e >> 18);
    return e;
}


int rand_r(unsigned *seedp)
{
    *seedp = mersenne_twister(*seedp & 0xf, *seedp) & 0x7fffffff;
    return *seedp;
}


static unsigned seed = 1;

int rand(void)
{
    return rand_r(&seed);
}


void srand(unsigned s)
{
    seed = s;
}
