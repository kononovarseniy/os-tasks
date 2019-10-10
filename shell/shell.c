#include <errno.h>
#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <unistd.h> // dup2, close, fork, execvp
#include <fcntl.h> // open
#include <stdio.h> // perror, *printf
#include <string.h> // strlen
#include "shell.h"

#define DEBUG

char *infile, *outfile, *appfile;
struct command cmds[MAXCMDS];
char bkgrnd;

int move_fd(int old_fd, int new_fd) {
    if (old_fd == new_fd)
        return 0;

    int dup_res;
    do {
        dup_res = dup2(old_fd, new_fd);
        if (dup_res == -1 && errno != EINTR)
            return -1;
    } while(dup_res == -1);

    if (close(old_fd))
        perror("Cannot close old file descriptor");

    return 0;
}

int open_files(int *in_fd, int *out_fd) {
    if (infile) {
        *in_fd = open(infile, O_RDONLY);
        if (*in_fd == -1) {
            perror("Cannot open file for reading");
            return -1;
        }
    }
    if (appfile) {
        *out_fd = open(appfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (*out_fd == -1) {
            perror("Cannot open file for append");
            return -1;
        }
    } else if (outfile) {
        *out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (*out_fd == -1) {
            perror("Cannot open file for writing");
            return -1;
        }
    }
    return 0;
}

void close_files(int in_fd, int out_fd) {
    if (in_fd != -1 && close(in_fd))
        perror("Cannot close input file");
    if (out_fd != -1 && close(out_fd))
        perror("Cannot close output file");
}

int main(int argc, char *argv[])
{
    register int i;
    char line[1024];      /*  allow large command lines  */
    char msg[50];
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
            if (infile)
                fprintf(stderr, "infile: %s\n", infile);
            if (outfile)
                fprintf(stderr, "outfile: %s\n", outfile);
            if (appfile)
                fprintf(stderr, "appfile: %s\n", appfile);
        }
#endif

        // Open files before starting any commands
        int in_fd = -1, out_fd = -1;
        if (open_files(&in_fd, &out_fd) == 0) {
            for (i = 0; i < ncmds; i++) {
                pid_t child = fork();
                if (child == -1) {
                    // Fork failed
                    perror("fork");
                } else if (child == 0) {
                    // Child process

                    // Redirect I/O if needed
                    if (i == 0 && in_fd != -1 && move_fd(in_fd, 0) == -1) {
                        perror("Failed to redirect input");
                        return 1;
                    }
                    if (i == ncmds - 1 && out_fd != -1 && move_fd(out_fd, 1) == -1) {
                        perror("Failed to redirect output");
                        return 1;
                    }
                    // Execute command
                    if (execvp(cmds[i].cmdargs[0], cmds[i].cmdargs) == -1) {
                        perror("exec");
                        return 1;
                    }
                } else {
                    // Main process
                    if (bkgrnd) {
                        bg_count++;
                        snprintf(msg, sizeof(msg), "[%d]\n", child);
                        write(0, msg, strlen(msg) + 1);
                    } else {
                        if (waitpid(child, NULL, 0) == -1) {
                            perror("waitpid");
                        }
                    }
                }
            }
        }
        close_files(in_fd, out_fd);

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
                snprintf(msg, sizeof(msg), "[%d] Done\n", pid);
                write(0, msg, strlen(msg) + 1);
            }
        }

    }  /* close while */
}

/* PLACE SIGNAL CODE HERE */

