#include <unistd.h>


int main(int argc, char *argv[])
{
    (void)argc;


    pid_t ret;

    if ((ret = fork()))
        return (ret < 0);


    execvp(argv[1], &argv[1]);

    return 1;
}
