#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "line_list.h"
#include "read_line.h"

//#define DEBUG
void *load_file(const char *filename, size_t *size) {
    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1)
        return NULL;

    *size = lseek(fd, 0, SEEK_END);
    char *ptr = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == NULL)
        return NULL;

    if (close(fd))
        perror("WARNING: close failed");

    return ptr;
}

int unload_file(void *ptr, size_t len) {
    return !munmap(ptr, len);
}

int scan_file(char *ptr, size_t size, struct list *list) {
    struct line curr;
    curr.offset = 0;
    for (size_t i = 0; i < size; i++) {
        if (ptr[i] == '\n') {
            curr.len = i - curr.offset;
            if (!add_line(list, &curr))
                return 0;
            curr.offset = i + 1;
        }
    }
    return 1;
}

int print_line(char *ptr, struct line *line) {
    char *str = ptr + line->offset;

    if (fwrite(str, 1, line->len, stdout) < line->len || printf("\n") < 0) {
        fprintf(stderr, "Printing failed\n");
        return 0;
    }
    return 1;
}

int print_lines(char *ptr, struct list *table) {
    struct node *n = table->head;
    while (n != NULL) {
        if (!print_line(ptr, &n->line))
            return 0;
        n = n->next;
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

/*
 * Read non empty line that firt in size-2 characters (underlying read_line()
 * uses last char of null-terminated string to indicate the end of line).
 * Resulting string is null-terminated and do not contains '\n'.
 */
int input_valid_line(struct file *f, char *buf, size_t size) {
    for(;;) {
        printf("Enter line number or 0 to quit\n");
        ssize_t len;
        int too_long = 0;
        for(;;) {
            len = read_line(f, buf, size, 5);
            if (len == RL_FAIL || len == RL_EOF || len == RL_TIMEOUT)
                return len;

            if (len == 1) // Empty line
                continue;
            if (buf[len - 1] == '\n')
                break;
            else
                too_long = 1;
        }
        if (!too_long) {
            // Replace '\n' with '\0'
            buf[len - 1] = '\0';
            return 0;
        }
        printf("Input string is too long\n");
    }    
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("No file name specified\n");
        return 1;
    }

    const char *filename = argv[1];
    size_t size;
    char *ptr = load_file(filename, &size);
    if (ptr == NULL) {
        perror("Unable to open file");
        return 1;
    }

    int err = 0;
    struct list table;
    init_list(&table);
    if (!scan_file(ptr, size, &table))
        perror("File scan failed");
    else {
        struct file *f = make_buffered_file(0, 1024);

        while (!err) {
            int should_quit = 0;
            char input[22];
            switch (input_valid_line(f, input, 22)) {
                case RL_EOF:
                    should_quit = 1;
                    break;
                case RL_FAIL:
                    fprintf(stderr, "input_valid_line failed\n");
                    err = 1;
                    continue;
                case RL_TIMEOUT:
                    if (!print_lines(ptr, &table))
                        fprintf(stderr, "print_lines failed\n");
                    should_quit = 1;
                    break;
            }
            if (should_quit)
                break;

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

            if (!print_line(ptr, &node->line)) {
                fprintf(stderr, "print_line failed\n");
                err = 1;
                break;
            }
        }

        free_buffered_file(f);
    }
    free_list(&table);
    unload_file(ptr, size);

    return err;
}
