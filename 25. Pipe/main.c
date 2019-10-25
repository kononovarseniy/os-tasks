#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <unistd.h>  // fork, pipe
#include <string.h> // strlen
#include <stdio.h> // perror
#include <ctype.h> // toupper
#include <errno.h>

#define MSG_SIZE 32

void print_upper_case(char *msg) {
    size_t len = strlen(msg);
    for (size_t i = 0; i < len; i++) {
        msg[i] = toupper(msg[i]);
    }
    printf("%s\n", msg);
}

int writer_main(int ifd, int ofd) {
    static char msg[MSG_SIZE] = "Hello, world!!!";
    if (close(ifd)) {
        perror("writer: cannot close read end of pipe");
    }

    int success = 0;
    while (!success) {
        int res = write(ofd, msg, MSG_SIZE);
        if (res == -1 && res != EAGAIN && res != EINTR) {
            perror("write failed");
            break;
        }
        success = 1;
    }

    if (close(ofd)) {
        perror("writer: cannot close write end of pipe");
    }

    return success ? 0 : 1;
}

int reader_main(int ifd, int ofd) {
    static char msg[MSG_SIZE];
    if (close(ofd)) {
        perror("reader: cannot close write end of pipe");
    }

    int success = 0;
    while (!success) {
        int res = read(ifd, msg, MSG_SIZE);
        if (res == -1 && res != EAGAIN && res != EINTR) {
            perror("read failed");
            break;
        }
        success = 1;
    }

    if (success)
        print_upper_case(msg);

    if (close(ifd)) {
        perror("reader: cannot close read end of pipe");
    }

    return success ? 0 : 1;
}

int main(int argc, char *const argv[]) {
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t writer = -1, reader = -1;

    // Start writer
    if ((writer = fork()) == -1) {
        perror("Fork failed (writer)");
    } else if (writer == 0) {
        return writer_main(fds[0], fds[1]);
    }

    // Start reader if writer succesfully started
    if (writer != -1) {
        if ((reader = fork()) == -1) {
            perror("Fork failed (reader)");
        } else if (reader == 0) {
            return reader_main(fds[0], fds[1]);
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
