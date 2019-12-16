#ifndef COMMAND_QUEUE_H_INCLUDED
#define COMMAND_QUEUE_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#define CMD_NOP 0

struct command {
    uint8_t cmd;
    uint8_t arg;
};

struct cmd_queue;

struct cmd_queue *make_cmd_queue(size_t cap);

void free_cmd_queue(struct cmd_queue *q);

size_t cmdq_length(struct cmd_queue *q);

int cmdq_enqueue(struct cmd_queue *q, struct command cmd);

struct command cmdq_dequeue(struct cmd_queue *q);

#endif
