#include <string.h> // strlen
#include <stdio.h> // perror
#include <ctype.h> // toupper

#define MSG_SIZE 32

void print_upper_case(char *msg) {
    size_t len = strlen(msg);
    for (size_t i = 0; i < len; i++) {
        msg[i] = toupper(msg[i]);
    }
    printf("%s\n", msg);
}

int reader_main() {
    static char msg[MSG_SIZE];
    if (fgets(msg, MSG_SIZE, stdin) == NULL) {
        fprintf(stderr, "fgets failed\n");
        return 1;
    }

    print_upper_case(msg);

    return 0;
}

int main(int argc, char *const argv[]) {
    if (argc == 2 && (strcmp(argv[1], "read")) == 0) {
        return reader_main();    
    }

    static char cmd[256];
    snprintf(cmd, sizeof(cmd), "'%s' read", argv[0]);

    FILE *out = popen(cmd, "w");
    if (out == NULL) {
        perror("popen");
        return 1;
    }

    fprintf(out, "Hello, world!!!");

    if (pclose(out)) {
        perror("pclose");
        return 1;
    }

    return 0;
}
