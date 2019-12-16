#include "round_buffer.h"

#include <stdlib.h> // malloc
#include <string.h>
#include <stdint.h>

struct round_buffer {
    size_t offset;
    size_t length;
    size_t capacity;
    uint8_t *buffer;
};

struct round_buffer *buf_create(size_t capacity) {
    void *ptr = malloc(capacity);
    if (ptr == NULL)
        return NULL;

    struct round_buffer *res = malloc(sizeof(struct round_buffer));
    if (res == NULL) {
        free(ptr);
        return NULL;
    }

    res->offset = 0;
    res->length = 0;
    res->capacity = capacity;
    res->buffer = ptr;

    return res;
}

void buf_destroy(struct round_buffer *buf) {
    free(buf->buffer);
    free(buf);
}

int buf_full(const struct round_buffer *buf) {
    return buf->length == buf->capacity;
}

int buf_empty(const struct round_buffer *buf) {
    return buf->length == 0;
}

size_t buf_data_length(const struct round_buffer *buf) {
    return buf->length;
}

size_t buf_free_length(const struct round_buffer *buf) {
    return buf->capacity - buf->length;
}

int buf_capacity(const struct round_buffer *buf) {
    return buf->capacity;
}

int buf_writing_iov(const struct round_buffer *buf, struct iovec *iov) {
    if (buf->length == buf->capacity)
        return 0;

    int cnt = 0;
    size_t part_start = buf->offset + buf->length;
    if (part_start < buf->capacity) {
        iov[cnt].iov_base = buf->buffer + part_start;
        iov[cnt].iov_len = buf->capacity - part_start;
        cnt++;
    }

    if (buf->offset > 0) {
        iov[cnt].iov_base = buf->buffer;
        iov[cnt].iov_len = buf->offset;
        cnt++;
    }

    return cnt;
}

int buf_reading_iov(const struct round_buffer *buf, struct iovec *iov) {
    if (buf->length == 0)
        return 0;

    size_t end = buf->offset + buf->length;
    if (end <= buf->capacity) {
        iov[0].iov_base = buf->buffer + buf->offset;
        iov[0].iov_len = buf->length;
        return 1;
    }

    iov[0].iov_base = buf->buffer + buf->offset;
    iov[0].iov_len = buf->capacity - buf->offset;

    iov[1].iov_base = buf->buffer;
    iov[1].iov_len = end - buf->capacity;
    return 2;
}

size_t buf_advance_read_ptr(struct round_buffer *buf, size_t length) {
    size_t max = buf_data_length(buf);
    if (max < length)
        length = max;

    if (length == buf->length) {
        buf->length = 0;
        buf->offset = 0;
    } else {
        buf->length -= length;
        buf->offset = (buf->offset + length) % buf->capacity;
    }
    return length;
}

size_t buf_advance_write_ptr(struct round_buffer *buf, size_t length) {
    size_t max = buf_free_length(buf);
    if (max < length)
        length = max;

    buf->length += length;

    return length;
}

int buf_peek_byte(struct round_buffer *buf) {
    if (buf_empty(buf))
        return -1;

    return buf->buffer[buf->offset];
}

ssize_t buf_peek(struct round_buffer *buf, void *ptr, size_t len) {
    struct iovec iov[2];
    int cnt = buf_reading_iov(buf, iov);
    if (cnt == 0)
        return -1;

    size_t res = 0;
    for (int i = 0; i < cnt && len > 0; i++) {
        size_t l = len;
        if (iov[i].iov_len < len)
            l = iov[i].iov_len;
        memcpy(ptr, iov[i].iov_base, l);
        ptr += l;
        len -= l;
        res += l;
    }
    return res;
}

ssize_t buf_write(int fd, struct round_buffer *buf) {
    struct iovec iov[2];
    int cnt = buf_reading_iov(buf, iov);
    if (cnt == 0)
        return -2;

    ssize_t res = writev(fd, iov, cnt);
    if (res != -1)
        buf_advance_read_ptr(buf, res);

    return res;
}

ssize_t buf_read(int fd, struct round_buffer *buf) {
    struct iovec iov[2];
    int cnt = buf_writing_iov(buf, iov);
    if (cnt == 0)
        return -2;

    ssize_t res = readv(fd, iov, cnt);
    if (res != -1)
       buf_advance_write_ptr(buf, res);

    return res;
}
