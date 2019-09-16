#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

struct line {
    off_t offset;
    off_t len;
};

struct list {
    struct line line;
    struct list *next;
};

#define BUF_SIZE 1024
char buf[BUF_SIZE];

struct list *make_list(struct line *line, struct list *next) {
    struct list *node = (struct list *) malloc(sizeof(struct list));
    if (node == NULL)
        return NULL;
    node->line = *line;
    node->next = next;
    return node;
}

void free_list(struct list *head) {
    while (head != NULL) {
        struct list *next = head->next;
        free(head);
        head = next;
    }
}

int fd;
struct list *head = NULL, *last = NULL;

int add_line(struct line *line) {
    struct list *new = make_list(line, NULL);
    if (new == NULL)
        return 0;

    if (head == NULL)
        last = head = new;
    else
        last = last->next = new;

    return 1;
}

void print_lines() {
    int i = 1;
    struct list *c = head;
    while (c != NULL) {
        printf("Line #%d at %ld has length %ld\n", i,
                (long) c->line.offset,
                (long) c->line.len);

        c = c->next;
        i++;
    }
}

void print_line(int ind) {
    struct list *c = head;
    for (; ind; ind--, c = c->next) {
        if (c == NULL) {
            fprintf(stderr, "Line index is out of range\n");
            return;
        }
    }
    // TODO: print line
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("No file name specified");
        return 1;
    }
    
    const char *filename = argv[1];

    if ((fd = open(filename, O_RDONLY)) == -1) {
        perror("Unable to open file");
        return 1;
    }

    int success = 1;
    int new_line = 1;
    struct line curr;
    while (success) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        ssize_t bytes_read = read(fd, buf, BUF_SIZE);
        if (!bytes_read) {
            if (!new_line) {
                curr.len = pos - curr.offset;
                if (!add_line(&curr)) {
                    perror("Unable to record line position");
                    success = 0;
                    continue;
                }
            }
            break; // EOF
        }
        if (bytes_read == -1) {
            perror("Error while readig file");
            success = 0;
            continue;
        }
        for (size_t o = 0; o < bytes_read; o++) {
            if (new_line) {
                curr.offset = pos + o;
                new_line = 0;
            }
            if (buf[o] == '\n') {
                curr.len = pos + o - curr.offset;
                new_line = 1;
                if (!add_line(&curr)) {
                    perror("Unable to record line position");
                    success = 0;
                    continue;
                }
            }
        }
        pos += bytes_read;
    }

    if (success) {
        print_lines();
        // char num_buf[11];
        // TODO: input line numer
        print_line(94);
    }

    if (close(fd)) {
        perror("Unable to close file");
        return 1;
    }
    
    return 0;
}
