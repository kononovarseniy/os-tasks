#ifndef READ_LINE_H
#define READ_LINE_H

#include <sys/types.h>

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

struct buffer *make_buf(size_t cap);
void free_buf(struct buffer *buf);

/*
 * Copy line from buffer.
 * Copies at most size characters from buffer including line-break.
 * This function doesn't instert line terminator.
 * Returns the number of copied characters. 
 */
size_t buf_consume_line(struct buffer *buf, char *res, size_t size);

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
ssize_t read_line(struct file *f, char *res, size_t size);

#endif
