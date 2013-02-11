#include <stdlib.h>


extern char **environ;


int clearenv(void)
{
    // Memleak für alle Variablen. Aber wenn wer mit putenv rumgemurkst hat,
    // dann kann man halt nicht free()en. Ansonsten sollte clearenv() ja auch
    // nicht so oft aufgerufen werden und so viel Speicher ist es ja nun auch
    // nicht.
    free(environ);
    environ = NULL;

    return 0;
}
