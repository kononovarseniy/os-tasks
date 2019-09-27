#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "line_list.h"
#include "read_line.h"

//#define DEBUG

#define BUF_SIZE 1024
char buf[BUF_SIZE];

int scan_file(int fd, struct list *list) {
    int new_line = 1;
    struct line curr;
    int success = 1;
    for(;;) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        ssize_t bytes_read = read(fd, buf, BUF_SIZE);
        if (bytes_read == -1) {
            perror("Error while readig file");
            success = 0;
            break;
        }
        if (!bytes_read) {
            if (!new_line) {
                curr.len = pos - curr.offset;
                if (!add_line(list, &curr)) {
                    perror("Unable to record line position");
                    success = 0;
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
                    break;
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

int print_lines(int fd, struct list *table) {
    struct node *n = table->head;
    while (n != NULL) {
        if (!print_line(fd, &n->line))
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
    int fd;

    if ((fd = open(filename, O_RDONLY)) == -1) {
        perror("Unable to open file");
        return 1;
    }

    int err = 0;
    struct list table;
    init_list(&table);
    if (scan_file(fd, &table)) {
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
                    if (!print_lines(fd, &table))
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

            if (!print_line(fd, &node->line)) {
                fprintf(stderr, "print_line failed\n");
                err = 1;
                break;
            }
        }

        free_buffered_file(f);
    }
    free_list(&table);

    if (close(fd)) {
        perror("Unable to close file");
        return 1;
    }

    return err;
}
