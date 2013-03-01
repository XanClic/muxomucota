#include <errno.h>
#include <ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>

static void tcp_send(int fd, const char *string)
{
    stream_send(fd, string, strlen(string), O_BLOCKING);
}

static char *my_fgets(char *s, int size, int stream)
{
    int cc = 0, i = 0, pi = 0;
    char collector[5];

    memset(s, 0, size);

    printf("\33[s\33[1;1f\33[44m%80c\33[0m\33[u", ' ');
    fflush(stdout);

    size--;
    while ((i < size) && (cc != '\n'))
    {
        stream_recv(stream, &cc, 1, O_BLOCKING);

        if (!pipe_get_flag(stream, F_READABLE))
            break;

        if ((cc & 0xC0) == 0xC0)
        {
            int expected = 0;
            if ((cc & 0xE0) == 0xC0)
                expected = 2;
            else if ((cc & 0xF0) == 0xE0)
                expected = 3;
            else if ((cc & 0xF8) == 0xF0)
                expected = 4;

            int j = 0;
            collector[j++] = cc;
            while (j < expected)
            {
                stream_recv(stream, &cc, 1, O_BLOCKING);
                collector[j++] = cc;
            }
            collector[j] = 0;

            printf("\33[s\33[1;%if\33[44m%s\33[0m\33[u", ++pi, collector);
            fflush(stdout);

            for (int k = 0; (k < j) && (k + i < size); k++)
                s[k + i] = collector[k];
            i += j;

            continue;
        }

        if (cc != '\n')
            printf("\33[s\33[1;%if\33[44m%c\33[0m\33[u", ++pi, cc);
        fflush(stdout);

        if ((cc == '\b') && i)
        {
            while (((s[--i] & 0xC0) == 0x80) && (i > 1))
                s[i] = 0;
            s[i] = 0;

            pi -= 2;

            continue;
        }

        if (cc != '\n')
            s[i++] = cc;
    }

    return s;
}

static int8_t kbd_thread_stack[1024];
static int quit = 0, joined = 0;
static char current_channel[64], current_nick[64];
static int con;
static char linebuf0_tbuf[256]; // FIXME: Das muss auch mit malloc gehen
static char *input_buffer;

static void kbd_thread(void *arg)
{
    (void)arg;

    pipe_set_flag(STDIN_FILENO, F_ECHO, 0);

    while (!quit)
    {
        my_fgets(input_buffer, 255, STDIN_FILENO);

        if (*input_buffer == '/')
        {
            char *cmd = strtok(input_buffer, " ");

            if (!strcmp(cmd, "/quit"))
            {
                printf("Verlasse den Server...\n");
                tcp_send(con, "QUIT :µxoµcota\r\n");
                quit = 1;
            }
            else if (!strcmp(cmd, "/join"))
            {
                if (joined)
                    printf("Es kann nur ein Channel betreten werden.\n");
                else
                {
                    memset(current_channel, 0, 64);
                    strncpy(current_channel, strtok(NULL, " "), 63);
                    char cbuf[80];
                    sprintf(cbuf, "JOIN %s\r\n", current_channel);
                    tcp_send(con, cbuf);
                    joined = 1;
                }
            }
            else if (!strcmp(cmd, "/part"))
            {
                if (!joined)
                    printf("Es muss zuerst ein Channel betreten werden.\n");
                else
                {
                    char pbuf[90];
                    sprintf(pbuf, "PART %s :paloxena3.5\r\n", current_channel);
                    tcp_send(con, pbuf);
                    joined = 0;
                }
            }
            else if (!strcmp(cmd, "/nick"))
            {
                printf("*** %s nennt sich jetzt ", current_nick);

                char nbuf[80];
                memset(current_nick, 0, 64);
                strncpy(current_nick, strtok(NULL, " "), 63);
                sprintf(nbuf, "NICK %s\r\n", current_nick);
                tcp_send(con, nbuf);

                printf("%s.\n", current_nick);
            }
            else if (!strcmp(cmd, "/me"))
            {
                char sbuf[256];
                sprintf(sbuf, "PRIVMSG %s :\1ACTION %s\1\r\n", current_channel, cmd + 4);
                tcp_send(con, sbuf);

                printf("* %s \33[1;34m%s\33[0m\n", current_nick, cmd + 4);
            }
            else
                printf("Unbekannter Befehl \"%s\".\n", cmd + 1);
        }
        else
        {
            if (!joined)
                printf("Es muss zuerst ein Channel betreten werden.\n");
            else
            {
                char sbuf[256];

                snprintf(sbuf, 256, "PRIVMSG %s :%s\r\n", current_channel, input_buffer);
                tcp_send(con, sbuf);

                printf("<%s> %s\n", current_nick, input_buffer);
            }
        }
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Aufruf: palirc <server>\n");
        return 1;
    }

    const char *server = argv[1];

    printf("Löse %s auf...\n", server);

    char sbuf[strlen(server) + 5];
    sprintf(sbuf, "(dns)/%s", server);
    int dns = create_pipe(sbuf, O_RDONLY);

    if (dns < 0)
    {
        fprintf(stderr, "%s: Konnte %s nicht öffnen: %s\n", argv[0], sbuf, strerror(errno));
        return 1;
    }

    size_t len = pipe_get_flag(dns, F_PRESSURE);
    if (!len)
    {
        destroy_pipe(dns, 0);
        fprintf(stderr, "%s: Konnte %s nicht auflösen: %s\n", argv[0], server, strerror(errno));
        return 1;
    }

    char ipbuf[16];
    memset(ipbuf, 0, 16);

    stream_recv(dns, ipbuf, len, O_BLOCKING);
    destroy_pipe(dns, 0);

    printf("%s erhalten. Verbindung wird hergestellt...\n", ipbuf);

    char tcpbuf[strlen(ipbuf) + 12];
    sprintf(tcpbuf, "(tcp)/%s:6667", ipbuf);

    con = create_pipe(tcpbuf, O_RDWR);

    if (con < 0)
    {
        fprintf(stderr, "%s: Konnte keine Verbindung zu %s aufbauen: %s\n", argv[0], tcpbuf, strerror(errno));
        return 1;
    }

    input_buffer = malloc(256);

    pid_t pid = create_thread(&kbd_thread, &kbd_thread_stack[1020], NULL);
    if (!pid)
    {
        destroy_pipe(con, 0);
        fprintf(stderr, "%s: Konnte Eingabethread nicht erstellen: %s\n", argv[0], strerror(errno));
        return 1;
    }

    strcpy(current_nick, "palirc-user");

    tcp_send(con, "NICK palirc-user\r\n");
    tcp_send(con, "USER palirc palirc xanclic.is-a-geek.org :palirc\r\n");

    char *recbuf = malloc(2048);
    char *linebufs[128];
    char *incomplete_line = NULL;

    while (!quit)
    {
        printf("\33[s\33[1;1f\33[44m%-80s\33[0m\33[u", input_buffer);
        fflush(stdout);

        memset(recbuf, 0, 2048);

        while (!pipe_get_flag(con, F_PRESSURE) && !quit)
            yield();

        if (quit)
            break;

        stream_recv(con, recbuf, 2047, O_NONBLOCK);

        if (!strlen(recbuf))
            continue;

        int last_line_complete = 1;
        if (recbuf[strlen(recbuf) - 1] != '\n')
            last_line_complete = 0;

        int start = 0;
        if (incomplete_line != NULL)
        {
            start = 1;
            const char *begin = strtok(recbuf, "\n");
            linebufs[0] = linebuf0_tbuf;
            strcpy(linebufs[0], incomplete_line);
            strcat(linebufs[0], begin);

            free(incomplete_line);

            len = strlen(linebufs[0]) - 1;
            if (linebufs[0][len] == '\r')
                linebufs[0][len] = 0;
        }

        incomplete_line = NULL;

        int i;
        for (i = start; i < 127; i++)
        {
            if (!i)
                linebufs[0] = strtok(recbuf, "\n");
            else
                linebufs[i] = strtok(NULL, "\n");

            if (linebufs[i] == NULL)
                break;
            else
            {
                if (!last_line_complete)
                    incomplete_line = linebufs[i];

                len = strlen(linebufs[i]) - 1;
                if (linebufs[i][len] == '\r')
                    linebufs[i][len] = 0;
            }
        }
        linebufs[i] = NULL;
        if (!last_line_complete)
            linebufs[i - 1] = NULL;

        if (!last_line_complete)
            incomplete_line = strdup(incomplete_line);

        for (i = 0; linebufs[i] != NULL; i++)
        {
            char *cmd = strtok(linebufs[i], " ");
            if (!strcmp(cmd, "PING"))
            {
                sprintf(recbuf, "PONG %s\r\n", strtok(NULL, " "));
                tcp_send(con, recbuf);
            }
            else if (!strcmp(cmd, "NOTICE"))
                printf("[Notice] %s\n", strchr(cmd + strlen(cmd) + 1, ':') + 1);
            else if (*cmd == ':')
            {
                char *name = &cmd[1];
                cmd = strtok(NULL, " ");
                char *dest = strtok(NULL, " ");
                char *msg = dest + strlen(dest) + 1;
                if (*msg == ':')
                    msg++;
                else
                    msg = strtok(NULL, " ");

                char *exc = strchr(name, '!');
                if (exc != NULL)
                    *exc = 0;

                if (!strcmp(cmd, "NOTICE"))
                    printf("[Notice] -%s- %s\n", name, msg);
                else if (!strcmp(cmd, "PRIVMSG"))
                {
                    if (*msg == 1) // CTCP
                    {
                        *strchr(msg + 1, 1) = 0;
                        if (!strncmp(msg + 1, "ACTION ", 7))
                            printf("* %s \33[1;34m%s\33[0m\n", name, msg + 8);
                        else
                            printf("*** CTCP-%s-Anfrage von %s erhalten.\n", msg + 1, name);
                    }
                    else
                    {
                        if (*dest != '#') // Query
                            printf("\33[0;1m");
                        printf("<%s> ", name);
                        if (*dest != '#')
                            printf("\33[0m");
                        puts(msg);
                    }
                }
                else if (!strcmp(cmd, "MODE"))
                    printf("*** %s setzt Modus von %s %s\n", name, dest, msg);
                else if (!strcmp(cmd, "PART"))
                    printf("*** %s hat den Kanal %s verlassen (%s).\n", name, dest, msg);
                else if (!strcmp(cmd, "QUIT"))
                    printf("*** %s hat den Server verlassen.\n", name);
                else if (!strcmp(cmd, "KICK"))
                {
                    char *real_msg = msg + strlen(msg) + 1;
                    if (*real_msg == ':')
                        real_msg++;
                    else
                        real_msg = strtok(NULL, " ");
                    printf("*** %s hat %s aus %s gekickt (%s).\n", name, msg, dest, real_msg);
                }
                else if (!strcmp(cmd, "JOIN"))
                {
                    if (*dest == ':')
                        dest++;
                    printf("*** %s hat den Kanal %s betreten.\n", name, dest);
                }
                else if (atoi(cmd))
                {
                    switch (atoi(cmd))
                    {
                        case 332:
                            printf("*** Thema von %s: %s\n", msg, msg + strlen(msg) + 2);
                            break;
                        case 352:
                            strtok(NULL, " "); strtok(NULL, " "); strtok(NULL, " ");
                            printf("Benutzer: %s\n", strtok(NULL, " "));
                            break;
                        case 353:
                            msg[strlen(msg)] = ' ';

                            char *usr = strtok(NULL, " ");
                            printf("*** Benutzer in %s:", usr);
                            usr += strlen(usr) + 1;
                            if (*usr == ':')
                                usr++;
                            else if (strchr(usr, ' ') != NULL)
                                *strchr(usr, ' ') = 0;

                            while ((usr = strtok(usr, " ")) != NULL)
                            {
                                printf(" [ %s ]", usr);
                                usr = NULL;
                            }
                            printf("\n");
                            break;
                        case 372:
                        case 375:
                        case 376:
                            printf("[MOTD] %s\n", msg);
                            break;
                        case 315:
                        case 366:
                            break;
                        default:
                            printf("[Notice] %s\n", msg);
                    }
                }
            }
        }
    }

    destroy_pipe(con, 0);

    return 0;
}
