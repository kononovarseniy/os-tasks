#include "command_queue.h"

#include <errno.h>
#include <stdio.h>

struct cmd_queue {
    size_t cap;
    size_t offset, length;
    struct command *queue;
};

struct cmd_queue *make_cmd_queue(size_t cap) {
    if (cap == 0) {
        errno = EINVAL;
        return NULL;
    }
    struct command *queue = malloc(cap * sizeof(struct command));
    if (queue == NULL)
        return NULL;

    struct cmd_queue *q = malloc(sizeof(struct cmd_queue));
    if (q == NULL) {
        free(queue);
        return NULL;
    }

    q->cap = cap;
    q->offset = 0;
    q->length = 0;
    q->queue = queue;

    return q;
}

void free_cmd_queue(struct cmd_queue *q) {
    if (q == NULL)
        return;
    free(q->queue);
    free(q);
}

size_t cmdq_length(struct cmd_queue *q) {
    return q->length;
}

int cmdq_enqueue(struct cmd_queue *q, struct command cmd) {
    if (q->length == q->cap)
        return 0;

    q->queue[(q->offset + q->length) % q->cap] = cmd;
    q->length++;

    return 1;
}

struct command cmdq_dequeue(struct cmd_queue *q) {
    struct command res;
    if (q->length == 0) {
        res.cmd = CMD_NOP;
        res.arg = 0;
        return res;
    }

    res = q->queue[q->offset++];
    if (q->offset >= q->cap)
        q->offset %= q->cap;
    q->length--;
    return res;
}
