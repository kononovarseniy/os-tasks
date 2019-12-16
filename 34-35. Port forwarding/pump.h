#ifndef PUMP_H_INCLUDED
#define PUMP_H_INCLUDED

#include "manager.h"

#define MAX_CONNECTIONS 256

struct pump;

typedef void (*command_handler)(void *arg, uint8_t cmd, uint8_t cmd_arg);

struct pump *make_pump(size_t cmd_queue_size, command_handler handler, void *handler_argument);

void free_pump(struct pump *p);

int send_command(struct pump *pump, uint8_t cmd, uint8_t arg);

void pump_transfer(struct pump *pump, struct connection **connections);

#endif
