#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <unistd.h>  // fork, execvp
#include <stdlib.h> // clearenv, putenv
#include <stdio.h> // perror

extern char **environ;
int my_execvpe(const char *file, char *const argv[], char *const envp[]) {
    environ = NULL;
    while (*envp != NULL)
        if (putenv(*envp++))
            return -1;

    return execvp(file, argv);
}

int main(int argc, char *const argv[]) {
    pid_t child = fork();
    if (child == -1) {
        perror("Fork failed");
        return 1;
    } else if (child == 0) {
        char *const args[] = {
            "env",
            NULL
        };
        char *const envp[] = {
            "ABC=hello",
            "PATH=/usr/bin",
            NULL
        };
        if (my_execvpe("env", args, envp) == -1) {
            perror("my_execvpe failed");
            return 1;
        }
    } else {
        if (waitpid(child, NULL, 0) == -1) {
            perror("waitpid failed");
            return 1;
        }
    }
    return 0;
}
