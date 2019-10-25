#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <fcntl.h> // fcntl
#include <unistd.h>  // fork, pipe, dup2
#include <string.h> // strlen
#include <stdlib.h> // system
#include <stdio.h> // perror
#include <ctype.h> // toupper
#include <errno.h>

int main(int argc, char *const argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: cel filename");
        return 1;
    }
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1) {
        perror("Cannot set close-on-exec");
    }
    if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
        perror("Cannot set close-on-exec");
    }

    pid_t writer = -1, reader = -1;

    if ((writer = fork()) == -1) {
        perror("Fork failed (grep)");
    } else if (writer == 0) {
        if (dup2(fds[1], 1) == -1) {
            perror("writer: cannot redirect output");
            return 1;
        }

        execlp("grep", "grep", argv[1], "-e", "^\\s*$", NULL);
        perror("exec grep");
        return 1;
    }

    // Start reader if writer succesfully started
    if (writer != -1) {
        if ((reader = fork()) == -1) {
            perror("Fork failed (wc)");
        } else if (reader == 0) {
            if (dup2(fds[0], 0) == -1) {
                perror("reader: cannot redirect input");
                return 1;
            }
            execlp("wc", "wc", "-l", NULL);
            perror("exec wc");
            return 1;
        }
    }

    if (close(fds[0])) {
        perror("Cannot close write end of pipe");
    }
    if (close(fds[1])) {
        perror("Cannot close read end of pipe");
    }

    if (writer != -1 && waitpid(writer, NULL, 0) == -1) {
        perror("waitpid failed (writer)");
    }
    if (reader != -1 && waitpid(reader, NULL, 0) == -1) {
        perror("waitpid failed (reder)");
    }

    return 0;
}
