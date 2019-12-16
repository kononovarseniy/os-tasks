#ifndef CONTROLLER_H_INCLUDED
#define CONTROLLER_H_INCLUDED

#include "manager.h"
#include "pump.h"

#define CMD_NEW 1
#define CMD_CLOSE 2
#define CMD_CLOSE_SRC_TO_DST 3
#define CMD_CLOSE_DST_TO_SRC 4
#define CMD_ACK 5

struct controller;

struct controller *
start_controller(size_t buf_size, int accepting, struct sockaddr_in *listen_addr, struct sockaddr_in *addr, int backlog);

void destroy_controller(struct controller *c);

int update(struct controller *controller);

#endif //CONTROLLER_H_INCLUDED
