#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s filename editor\n", argv[0]);
        return 1;
    }
    char *filename = argv[1];
    char *editor = argv[2];
    
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Open file failed");
        return 1;
    }
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_CUR;
    lock.l_start = 0;
    lock.l_len = 0; // Lock whole file
    int locked = 1;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Connot lock file for writing");
        locked = 0;
    }
    if (locked) {
        char cmd[256];
        snprintf(cmd, 256, "%s %s", editor, filename);
        int res = system(cmd);
        if (res == -1) {
            perror("Cannot run editor");
        } else {
            printf("Editor exited with status %d\n", res);
        }
        lock.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &lock) == -1) {
            perror("Unlock failed");
        }
    }
    if (close(fd)) {
        perror("close failed");
        return 1;
    }
    return 0;
}
