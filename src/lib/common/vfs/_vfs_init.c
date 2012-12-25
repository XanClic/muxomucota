#include <string.h>
#include <vfs.h>


void _vfs_init(void);

void _vfs_init(void)
{
    // Sonst ist das unitialisiert und im Zweifel sind also alle Einträge
    // belegt (Ergebnis: direkt EMFILE beim create_pipe).
    memset(_pipes, 0, sizeof(_pipes[0]) * __MFILE);
}
