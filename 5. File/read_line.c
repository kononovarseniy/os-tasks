#include "read_line.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

