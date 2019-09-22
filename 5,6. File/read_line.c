#include "read_line.h"

#include <sys/select.h>
#include <sys/timerfd.h>
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

int start_timer(time_t timeout) {
    int timer = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec ts;
    // No repeat
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    // Set timeout
    ts.it_value.tv_sec = timeout;
    ts.it_value.tv_nsec = 0;

    timerfd_settime(timer, 0, &ts, NULL);
    return timer;
}

ssize_t read_line(struct file *f, char *res, size_t size, time_t timeout) {
    char *start = res;
    int fd = f->fd;
    struct buffer *buf = f->buf;
    if (size == 0)
        return 0;
    size--; // Reserve space for '\0'.
    int timer = start_timer(timeout);
    int err = 0;
    while (size) {
        if (!buf->len) {
            ssize_t bytes_read;
            if (timeout == -1) {
                bytes_read = read(fd, buf->buf, buf->cap);
            } else {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(0, &rfds);
                FD_SET(timer, &rfds);
                int nfds = timer + 1;

                struct timeval tv;
                tv.tv_sec = timeout;
                tv.tv_usec = 0;

                int res = select(nfds, &rfds, NULL, NULL, &tv);
                if (res == -1) {
                    perror("read_line: select failed");
                    err = RL_FAIL;
                    break;
                }
                if (FD_ISSET(0, &rfds))
                    bytes_read = read(fd, buf->buf, buf->cap);

                if (FD_ISSET(timer, &rfds)) {
                    err = RL_TIMEOUT;
                    break;;
                }
            }
            if (bytes_read == 0) {
                if (res != start) { // String is not empty
                    *res++ = '\n';
                    break; // Next call to read_line will detect EOF.
                }
                err = RL_EOF;
                break;
            }

            if (bytes_read == -1) {
                perror("read_line: read failed");
                err = RL_FAIL;
                break;
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

    close(timer);
    if (err)
        return err;

    *res = '\0';
    return res - start;
}

