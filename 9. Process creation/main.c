#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <stdlib.h>  // system
#include <unistd.h> // fork
#include <stdio.h> // perror, *printf

#define CMD_BUF_SIZE 256

int main(int argc, char *const argv[]) {
    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }
    char *const file = argv[1];

    char cmd[CMD_BUF_SIZE];
    size_t res = snprintf(cmd, CMD_BUF_SIZE, "cat %s", file);
    if (res >= CMD_BUF_SIZE) {
        fprintf(stderr, "Filename is too long\n");
        return 1;
    }
    if (res < 0) {
        fprintf(stderr, "Failed to form command string\n");
        return 1;
    }
            
    pid_t child = fork();
    if (child == -1) {
        // Error
        perror("Fork failed");
        return 1;
    } else if (child == 0) {
        // Child process
        int status = system(cmd);
        if (status == -1) {
            perror("Child: cannot execute command");
            return 1;
        }
    } else {
        // Parent process
        for (int i = 0; i < 100; i++)
            printf("Parent: %d\n", i);
        int status;
        if (waitpid(child, &status, 0) == -1) {
            perror("Parent: waitpid failed");
            return 1;
        }
        if (WIFEXITED(status)) {
            printf("Parent: child exited with status %d\n", WEXITSTATUS(status));
        } else {
            printf("Parent: child terminated by sgnal\n");
        }
    }

    return 0;
}
