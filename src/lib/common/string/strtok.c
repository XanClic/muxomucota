#include <string.h>

char *strtok(char *string1, const char *string2)
{
    static char *next_token = NULL;
    char *tok;
    int slen = strlen(string2);

    if (string1 == NULL)
        string1 = next_token;

    if (string1 == NULL)
        return NULL;

    while ((tok = strstr(string1, string2)) == string1)
        string1 += slen;

    if (!*string1)
    {
        next_token = NULL;
        return NULL;
    }

    if (tok == NULL)
        next_token = NULL;
    else
    {
        *tok = '\0';
        next_token = tok + slen;
    }

    return string1;
}
