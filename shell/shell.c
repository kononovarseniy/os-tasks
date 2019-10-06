#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include "shell.h"

//#define DEBUG

char *infile, *outfile, *appfile;
struct command cmds[MAXCMDS];
char bkgrnd;

int main(int argc, char *argv[])
{
    register int i;
    char line[1024];      /*  allow large command lines  */
    int ncmds;
    char prompt[50];      /* shell prompt */
    int bg_count = 0;

    /* PLACE SIGNAL CODE HERE */

    sprintf(prompt,"%s>", argv[0]);

    while (promptline(prompt, line, sizeof(line)) > 0) {    /* until eof  */
        if ((ncmds = parseline(line)) <= 0)
            continue;   /* read next line */

#ifdef DEBUG
        {
            int i, j;
            for (i = 0; i < ncmds; i++) {
                for (j = 0; cmds[i].cmdargs[j] != (char *) NULL; j++)
                    fprintf(stderr, "cmds[%d].cmdargs[%d] = %s\n",
                            i, j, cmds[i].cmdargs[j]);
                fprintf(stderr, "cmds[%d].cmdflag = %o\n", i,
                        cmds[i].cmdflag);
            }
        }
#endif

        for (i = 0; i < ncmds; i++) {
            pid_t child = fork();
            if (child == -1) {
                perror("fork");
            } else if (child == 0) {
                if (execvp(cmds[i].cmdargs[0], cmds[i].cmdargs) == -1) {
                    perror("exec");
                    return 1;
                }
            } else {
                if (bkgrnd) {
                    bg_count++;
                    printf("[%d]\n", child);
                } else {
                    if (waitpid(child, NULL, 0) == -1) {
                        perror("waitpid");
                    }
                }
            }
        }
        // Check children state
        while (bg_count > 0) {
            pid_t pid = waitpid(-1, NULL, WNOHANG);
            if (pid == -1) {
                perror("waitpid");
                break;
            } else if (pid == 0) {
                // No changes. Stop checking
                break;
            } else {
                bg_count--;
                printf("[%d] Done\n", pid);
            }
        }

    }  /* close while */
}

/* PLACE SIGNAL CODE HERE */

