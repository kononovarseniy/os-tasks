#ifndef ROUND_BUFFER_H
#define ROUND_BUFFER_H

#include <sys/types.h> // ssize_t
#include <sys/uio.h> // struct iovec

struct round_buffer;

struct round_buffer *buf_create(size_t capacity);
void buf_destroy(struct round_buffer *buf);

int buf_full(const struct round_buffer *buf);
int buf_empty(const struct round_buffer *buf);
size_t buf_data_length(const struct round_buffer *buf);
size_t buf_free_length(const struct round_buffer *buf);
int buf_capacity(const struct round_buffer *buf);

#define MAX_IOV_LEN 2
int buf_writing_iov(const struct round_buffer *buf, struct iovec *iov);
int buf_reading_iov(const struct round_buffer *buf, struct iovec *iov);

size_t buf_advance_read_ptr(struct round_buffer *buf, size_t length);
size_t buf_advance_write_ptr(struct round_buffer *buf, size_t length);

int buf_peek_byte(struct round_buffer *buf);
ssize_t buf_peek(struct round_buffer *buf, void *ptr, size_t len);
ssize_t buf_write(int fd, struct round_buffer *buf);
ssize_t buf_read(int fd, struct round_buffer *buf);

#endif
