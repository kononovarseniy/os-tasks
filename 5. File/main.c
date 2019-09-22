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

struct buffer {
    void *buf;
    size_t cap;
    size_t pos;
    size_t len;
};

struct file {
    int fd;
    struct buffer *buf;
};

struct buffer *make_buf(size_t cap) {
    void *buf = malloc(cap);
    if (buf == NULL)
        return NULL;

    struct buffer *res = malloc(sizeof(struct buffer));
    if (res == NULL)
        return NULL;

    res->buf = buf;
    res->cap = cap;
    res->pos = 0;
    res->len = 0;
    return res;
}

void free_buf(struct buffer *buf) {
    free(buf->buf);
    free(buf);
}

/*
 * Copy line from buffer.
 * Copies at most size characters from buffer including line-break.
 * This function doesn't instert line terminator.
 * Returns the number of copied characters. 
 */
size_t buf_consume_line(struct buffer *buf, char *res, size_t size) {
    size_t cnt = buf->len <= size ? buf->len : size;
    char *buf_start = buf->buf + buf->pos;
    for (size_t i = 0; i < cnt; i++) {
        if (buf_start[i] == '\n') {
            cnt = i + 1;
            break;
        }
    }

    memcpy(res, buf->buf + buf->pos, cnt);
    buf->pos += cnt;
    buf->len -= cnt;
    return cnt;
}

/*
 * Read line of text from file and stores it into the buffer pointed by res.
 * At most one less than size characters (counting line break) will be read.
 * End of file considered line break.
 * If the string is fully read the last character of res is '\n'.
 * Returns the length of read string,
 * RL_FAIL if an IO error occured,
 * RL_EOF if nothing can be read because end of file is reached. 
 */
#define RL_FAIL -1
#define RL_EOF  -2
ssize_t read_line(struct file *f, char *res, size_t size) {
    char *start = res;
    int fd = f->fd;
    struct buffer *buf = f->buf;
    if (size == 0)
        return 0;
    size--; // Reserve space for '\0'.
    while (size) {
        if (!buf->len) {
            ssize_t bytes_read = read(fd, buf->buf, buf->cap);
            if (bytes_read == 0) {
                if (res != start) { // String is not empty
                    *res++ = '\n';
                    break; // Next call to read_line will detect EOF.
                }
                return RL_EOF;
            }

            if (bytes_read == -1) {
                perror("read_line: read failed");
                return RL_FAIL;
            }
            buf->pos = 0;
            buf->len = bytes_read;
        }

        size_t cnt = buf_consume_line(buf, res, size);
        res += cnt;
        size -= cnt;
        if (res[-1] == '\n')
            break;
    }
    *res = '\0';
    return res - start;
}

/*
 * Read non empty line that firt in size-2 characters (underlying reqad_line()
 * uses last char of null-terminated string to indicate the end of line).
 * Resulting string is null-terminated and do not contains '\n'.
 */
int input_valid_line(struct file *f, char *buf, size_t size) {
    for(;;) {
        printf("Enter line number or 0 to quit\n");
        ssize_t len;
        int too_long = 0;
        for(;;) {
            len = read_line(f, buf, size);
            if (len == RL_FAIL || len == RL_EOF)
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
    if (scan_file(fd, &table)) {
        debug_print_lines();

        struct file f;
        f.fd = 0;
        f.buf = make_buf(1024);

        while (!err) {
            char input[22];
            switch (input_valid_line(&f, input, 22)) {
                case RL_EOF:
                    strcpy(input, "0");
                    break;
                case RL_FAIL:
                    fprintf(stderr, "input_valid_line failed\n");
                    err = 1;
                    continue;
            }

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

        free_buf(f.buf);
    }

    if (close(fd)) {
        perror("Unable to close file");
        return 1;
    }

    return err;
}
