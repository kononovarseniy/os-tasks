#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG

struct line {
    off_t offset;
    off_t len;
};

struct node {
    struct line line;
    struct node *next;
};

struct list {
    struct node *head;
    struct node *last;
};

#define BUF_SIZE 1024
char buf[BUF_SIZE];

struct node *make_node(struct line *line, struct node *next) {
    struct node *node = (struct node *) malloc(sizeof(struct node));
    if (node == NULL)
        return NULL;
    node->line = *line;
    node->next = next;
    return node;
}

void free_node_recursive(struct node *head) {
    while (head != NULL) {
        struct node *next = head->next;
        free(head);
        head = next;
    }
}

void init_list(struct list *list) {
    list->head = NULL;
    list->last = NULL;
}

void free_list(struct list *list) {
    free_node_recursive(list->head);
}

struct node *get_node(struct node *head, unsigned long offset) {
    while (offset-- && head != NULL)
        head = head->next;
    return head;
}

int add_line(struct list *list, struct line *line) {
    struct node *new = make_node(line, NULL);
    if (new == NULL)
        return 0;

    if (list->head == NULL)
        list->last = list->head = new;
    else
        list->last = list->last->next = new;

    return 1;
}

void debug_print_lines() {
#ifdef DEBUG
    int i = 1;
    struct node *c = head;
    while (c != NULL) {
        printf("Line #%d at %ld has length %ld\n", i,
                (long) c->line.offset,
                (long) c->line.len);

        c = c->next;
        i++;
    }
#endif
}

int scan_file(int fd, struct list *list) {
    int new_line = 1;
    struct line curr;
    int success = 1;
    while (success) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        ssize_t bytes_read = read(fd, buf, BUF_SIZE);
        if (bytes_read == -1) {
            perror("Error while readig file");
            success = 0;
            continue;
        }
        if (!bytes_read) {
            if (!new_line) {
                curr.len = pos - curr.offset;
                if (!add_line(list, &curr)) {
                    perror("Unable to record line position");
                    success = 0;
                    continue;
                }
            }
            break; // EOF
        }
        for (size_t o = 0; o < bytes_read; o++) {
            if (new_line) {
                curr.offset = pos + o;
                new_line = 0;
            }
            if (buf[o] == '\n') {
                curr.len = pos + o - curr.offset;
                new_line = 1;
                if (!add_line(list, &curr)) {
                    perror("Unable to record line position");
                    success = 0;
                    continue;
                }
            }
        }
        pos += bytes_read;
    }
    return success;
}

int print_line(int fd, struct line *line) {
    lseek(fd, line->offset, SEEK_SET);
    off_t count_left = line->len;
    while (count_left > 0) {
        ssize_t bytes_read = read(fd, buf, BUF_SIZE - 1);
        if (bytes_read == -1) {
            perror("Read failed");
            return 0; 
        }
        size_t cnt = count_left > bytes_read ? bytes_read : count_left;
        buf[cnt] = 0;
        count_left -= cnt;
        if (puts(buf) == EOF) {
            fprintf(stderr, "Write failed\n");
            return 0;
        }
    }
    return 1;
}

int str_to_long(const char *str, long *res) {
    char *end;
    errno = 0;
    *res = strtol(str, &end, 10);
    // String contains invalid characters or an error occured.
    if (*end || errno)
        return 0;
    return 1;
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("No file name specified\n");
        return 1;
    }
    
    const char *filename = argv[1];
    int fd;

    if ((fd = open(filename, O_RDONLY)) == -1) {
        perror("Unable to open file");
        return 1;
    }

    struct list table;
    if (scan_file(fd, &table)) {
        debug_print_lines();

        while (1) {
            printf("Enter line number or 0 to quit\n");
            char input[100];
            if (fgets(input, 100, stdin) == NULL) {
                fprintf(stderr, "Unable to read user input\n");
                break;
            }
            char *end;
            if ((end = strchr(input, '\n')) == NULL) {
                printf("Input is too long\n");
                continue;
            }
            *end = '\0';
            long num;
            if (!str_to_long(input, &num)) {
                printf("Input string is not a number or too long\n");
                continue;
            }

            if (num == 0)
                break;
            if (num < 0) {
                printf("Please enter positive number\n");
                continue;
            }
            struct node *node = get_node(table.head, num - 1);
            if (node == NULL) {
                printf("No such line\n");
                continue;
            }
            if (!print_line(fd, &node->line)) {
                fprintf(stderr, "print_line failed\n");
                break;
            }
        }
    }

    if (close(fd)) {
        perror("Unable to close file");
        return 1;
    }

    return 0;
}
