#include <errno.h>
#include <ipc.h>


int main(void)
{
    do
    {
        errno = 0;
        ipc_message(2);
    } while (errno == ESRCH);

    return 0;
}
