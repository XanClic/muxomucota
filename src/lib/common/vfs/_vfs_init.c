#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>


FILE *stdin  = &(FILE){ .fd = STDIN_FILENO  };
FILE *stdout = &(FILE){ .fd = STDOUT_FILENO };
FILE *stderr = &(FILE){ .fd = STDERR_FILENO };


void _vfs_init(void);

void _vfs_init(void)
{
    // Sonst ist das unitialisiert und im Zweifel sind also alle Einträge
    // belegt (Ergebnis: direkt EMFILE beim create_pipe).
    memset(_pipes, 0, sizeof(_pipes[0]) * __MFILE);
}
