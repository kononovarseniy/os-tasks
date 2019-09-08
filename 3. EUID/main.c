#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

extern char *tzname[];

void print_ids() {
    uid_t uid = getuid(), euid = geteuid();
    
    printf("uid: %d; euid: %d\n", uid, euid);
}

void open_file() {
    FILE *f;
    if ((f = fopen("file.txt", "r")) == NULL) {
        perror("Cannot open file");
        return;
    }
    
    char buf[100];
    if (fgets(buf, 100, f) == NULL) {
        printf("The file is empty\n");
    } else {
        printf("The file starts with:\n%s", buf);
    }
    
    if (fclose(f))
        perror("fclose failed");
    else
        printf("File successfuly closed\n");
}

int main()
{
    printf("===\n");
    print_ids();
    open_file();
    
    printf("===\n");
    setuid(getuid());
    print_ids();
    open_file();
    
    return 0;
}
