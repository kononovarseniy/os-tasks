#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <unistd.h> // fork, execvp
#include <stdio.h> // perror, printf

int main(int argc, char *const argv[]) {
    if (argc < 2) {
        printf("USAGE: %s command args...\n", argv[0]);
        return 1;
    }
    char *command = argv[1];

    pid_t child = fork();
    if (child == -1) { // Error
        perror("Fork failed");
        return 1;
    } else if (child == 0) { // Child
        // argv is null terminated
        if (execvp(command, argv + 1) == -1) {
           perror("Exec failed");
           return 1;
        }
    } else { // Parent
       int status;
       if (waitpid(child, &status, 0) == -1) {
           perror("Waitpid failed");
           return 1;
       }
       if (WIFEXITED(status)) {
           printf("The child process exited with status %d\n", WEXITSTATUS(status));
       } else {
           printf("The child process was terminated by a signal\n");
       }
    }
    return 0;
}
