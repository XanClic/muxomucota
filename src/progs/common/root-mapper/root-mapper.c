#include <drivers.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static struct entry
{
    struct entry *next;
    char *base, *dest;
} *entries = NULL;


bool service_is_symlink(const char *relpath, char *linkpath)
{
    struct entry *best_match = NULL;
    int best_match_len = -1;

    for (struct entry *e = entries; e != NULL; e = e->next)
    {
        int len = strlen(e->base);

        if ((len > best_match_len) && !strncmp(relpath, e->base, len) && (!relpath[len] || (relpath[len] == '/')))
        {
            best_match = e;
            best_match_len = len;
        }
    }

    if (best_match == NULL)
        return false;

    strcpy(linkpath, best_match->dest);
    strcat(linkpath, relpath + best_match_len);

    return true;
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: root-mapper <base path>=<destination>...\n");
        return 1;
    }


    struct entry **endptr = &entries;

    for (int i = 1; i < argc; i++)
    {
        if (strchr(argv[i], '=') == NULL)
        {
            fprintf(stderr, "Invalid argument \"%s\" ('=' expected).\n", argv[i]);
            return 1;
        }

        char *duped = strdup(argv[i]);

        struct entry *entry = malloc(sizeof(*entry));
        entry->base = strtok(duped, "=");
        entry->dest = strtok(NULL, "=");

        if (strtok(NULL, "="))
        {
            fprintf(stderr, "Invalid argument \"%s\" (too many '=').\n", argv[i]);
            return 1;
        }

        for (int j = strlen(entry->base) - 1; (j >= 0) && (entry->base[j] == '/'); j--)
            entry->base[j] = 0;

        *endptr = entry;
        endptr = &entry->next;
    }

    *endptr = NULL;


    daemonize("root", true);
}
