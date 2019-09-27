#ifndef READ_LINE_H
#define READ_LINE_H

#include <sys/types.h>

struct file;

struct file *make_buffered_file(int fd, size_t buf_size);
void free_buffered_file(struct file *f);

/*
 * Read line of text from file and stores it into the buffer pointed by res.
 * At most one less than size characters (counting line break) will be read.
 * End of file considered line break.
 * If the string is fully read the last character of res is '\n'.
 * If timeout is -1 read_line is blocking.
 * Returns the length of read string,
 * RL_FAIL if an IO error occured,
 * RL_EOF if nothing can be read because end of file is reached,
 * RL_TIMEOUT if temeout expired. 
 */
#define RL_FAIL     -1
#define RL_EOF      -2
#define RL_TIMEOUT  -3
ssize_t read_line(struct file *f, char *res, size_t size, time_t timeout);

#endif
