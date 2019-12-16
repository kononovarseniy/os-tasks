#include "pump.h"

#include "command_queue.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define END_BYTE 0x7E
#define ESC_BYTE 0x7F

struct pump {
    int sending_to;
    int current_index;
    void *cmd_handler_arg;
    command_handler cmd_handler;
    struct cmd_queue *cmd_queue;
};

struct buf_iter {
    struct iovec iov[MAX_IOV_LEN];
    int iov_cnt;
    int iov_ind;
    uint8_t *ptr;
    size_t rem;
};

static inline void prepare_buf_iter(struct buf_iter *iter) {
    iter->iov_ind = 0;
    iter->ptr = iter->iov[0].iov_base;
    iter->rem = iter->iov[0].iov_len;
}

static inline int buf_iter_next(struct buf_iter *iter) {
    iter->ptr++;
    if (--iter->rem == 0) {
        if (++iter->iov_ind == iter->iov_cnt)
            return 0;
        iter->rem = iter->iov[iter->iov_ind].iov_len;
        iter->ptr = iter->iov[iter->iov_ind].iov_base;
    }
    return 1;
}

static struct buf_iter get_read_iter(const struct round_buffer *buf) {
    struct buf_iter iter;
    iter.iov_cnt = buf_reading_iov(buf, iter.iov);
    prepare_buf_iter(&iter);
    return iter;
}

static struct buf_iter get_write_iter(const struct round_buffer *buf) {
    struct buf_iter iter;
    iter.iov_cnt = buf_writing_iov(buf, iter.iov);
    prepare_buf_iter(&iter);
    return iter;
}

/*
 * Read bytes from src, encode them and write to dst.
 * Function tries to encode maximum possible number of bytes.
 * Returns 1 when src is empty or when at least one byte encoded,
 * and 0 if there are not enough space in dst buffer.
 */
static int encode(
        struct round_buffer *src,
        struct round_buffer *dst,
        uint8_t connection_id) {
    if (buf_data_length(src) == 0)
        return 1;

    struct buf_iter src_it = get_read_iter(src);
    struct buf_iter dst_it = get_write_iter(dst);
    size_t src_total = 0, dst_total = 0;

    size_t dst_avail = buf_free_length(dst);
    int first = *src_it.ptr;
    if (first == -1 || dst_avail < 4 || (first == ESC_BYTE && dst_avail < 5))
        return 0;

    *dst_it.ptr = END_BYTE;
    buf_iter_next(&dst_it);
    dst_total++;
    *dst_it.ptr = connection_id;
    buf_iter_next(&dst_it);
    dst_total++;

    size_t dst_rem = dst_avail - 3;
    while (dst_rem > 0) {
        uint8_t b = *src_it.ptr;
        if (b == ESC_BYTE || b == END_BYTE) {
            if (dst_rem < 2)
                break;

            *dst_it.ptr = ESC_BYTE;
            buf_iter_next(&dst_it);
            dst_rem--;
            dst_total++;
        }
        *dst_it.ptr = b;
        dst_rem--;
        dst_total++;

        src_total++;
        buf_iter_next(&dst_it);
        if (!buf_iter_next(&src_it))
            break;
    }

    *dst_it.ptr = END_BYTE;
    dst_total++;

    buf_advance_read_ptr(src, src_total);
    buf_advance_write_ptr(dst, dst_total);
    return 1;
}

/*
 * Read bytes from src, decode them and write to dst.
 * Bytes are read until the END_BYTE or the end of src buffer or until dsc buffer is full
 */
static int decode(
        struct round_buffer *src,
        struct round_buffer *dst) {
    struct buf_iter src_it = get_read_iter(src);
    struct buf_iter dst_it = get_write_iter(dst);

    int end_reached = 0;
    size_t dst_rem = buf_free_length(dst);
    size_t src_total = 0;
    size_t dst_total = 0;
    while (dst_rem > 0) {
        uint8_t b = *src_it.ptr;
        src_total++;
        if (b == END_BYTE) {
            // No need to increment iterator
            end_reached = 1;
            break;
        }
        if (b == ESC_BYTE) {
            buf_iter_next(&src_it);
            b = *src_it.ptr;
            src_total++;
        }
        *dst_it.ptr = b;
        buf_iter_next(&dst_it);
        dst_total++;
        dst_rem--;
        if (!buf_iter_next(&src_it))
            break;
    }
    buf_advance_read_ptr(src, src_total);
    buf_advance_write_ptr(dst, dst_total);
    return end_reached;
}

static int encode_command(struct connection *tunnel, uint8_t cmd, uint8_t arg) {
    struct round_buffer *buf = tunnel->out_buf;
    size_t avail = buf_free_length(buf);
    if (avail < 5)
        return 0;

    struct buf_iter it = get_write_iter(buf);

    *it.ptr = END_BYTE;
    buf_iter_next(&it);

    *it.ptr = 0;
    buf_iter_next(&it);

    *it.ptr = cmd;
    buf_iter_next(&it);

    *it.ptr = arg;
    buf_iter_next(&it);

    *it.ptr = END_BYTE;
    buf_iter_next(&it);

    buf_advance_write_ptr(buf, 5);

    return 1;
}

static void recv_from_tunnel(struct pump *pump, struct connection *tunnel, struct connection **connections) {
    struct round_buffer *buf = tunnel->in_buf;
    if (buf_empty(buf))
        return;

    if (pump->sending_to != -1) {
        // Continue to send data from tunnel to connection until the end of the message.
        if (!decode(buf, connections[pump->sending_to]->out_buf))
            return;
        pump->sending_to = -1;
    }
    // If input buffer contains END_BYTE(0x7E), connection_id and at least one byte of data
    // It is a minimal required number of bytes to take a decision.
    size_t available = buf_data_length(buf);
    if (available > 3) {
        uint8_t start[3];
        if (buf_peek(buf, start, 3) == -1) {
            printf("PANIC!!!\n");
            exit(2);
        }

        if (start[0] != END_BYTE) {
            fprintf(stderr, "pump: protocol violation: wrong starting byte\n");
            // TODO: shutdown
            exit(2);
        }
        int conn_id = start[1];
        if (conn_id == 0) {
            int cmd = start[2];
            if (available < 5) {
                // Wait for more data
                return;
            }
            buf_advance_read_ptr(buf, 3);
            int arg = buf_peek_byte(buf);
            pump->cmd_handler(pump->cmd_handler_arg, cmd, arg);
            buf_advance_read_ptr(buf, 2);
        } else {
            if (connections[conn_id] == NULL) {
                fprintf(stderr, "pump: protocol violation: usage of closed connection\n");
                // TODO: shutdown
                exit(2);
            }
            buf_advance_read_ptr(buf, 2);
            if (!decode(buf, connections[conn_id]->out_buf)) {
                pump->sending_to = conn_id;
                return;
            }
        }
    }
}

int send_command(struct pump *pump, uint8_t cmd, uint8_t arg) {
    return cmdq_enqueue(pump->cmd_queue, (struct command) {.cmd = cmd, .arg=arg});
}

void pump_transfer(struct pump *pump, struct connection **connections) {
    struct connection *tunnel = connections[0];

    recv_from_tunnel(pump, tunnel, connections);

    // Commands have highest priority
    while (cmdq_length(pump->cmd_queue)) {
        struct command cmd = cmdq_dequeue(pump->cmd_queue);
        if (!encode_command(tunnel, cmd.cmd, cmd.arg)) {
            // Buffer is full
            return;
        }
    }

    int index = pump->current_index;
    for (int i = 0; i < MAX_CONNECTIONS - 1; i++) {
        int ind = (index + i) % 255 + 1;

        struct connection *c = connections[ind];
        if (c == NULL || !is_alive(c->state) || !is_alive(c->in_state))
            continue;
        if (!encode(c->in_buf, tunnel->out_buf, c->id)) {
            // Buffer is full
            pump->current_index = ind;
            break;
        }
    }
}

struct pump *make_pump(size_t cmd_queue_size, command_handler handler, void *handler_argument) {
    struct cmd_queue *q = make_cmd_queue(cmd_queue_size);
    if (q == NULL)
        return NULL;

    struct pump *pump = malloc(sizeof(struct pump));
    if (pump == NULL) {
        free_cmd_queue(q);
        return NULL;
    }
    pump->sending_to = -1;
    pump->current_index = 0;
    pump->cmd_handler_arg = handler_argument;
    pump->cmd_handler = handler;
    pump->cmd_queue = q;
    return pump;
}

void free_pump(struct pump *p) {
    if (p == NULL)
        return;
    free_cmd_queue(p->cmd_queue);
    free(p);
}
