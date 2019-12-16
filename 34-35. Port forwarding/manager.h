#ifndef POLLING_H
#define POLLING_H

#include <stdint.h>
#include <netinet/in.h>

#include "round_buffer.h"

#define CS_NEW ((uint8_t)1)
#define CS_EOF ((uint8_t)2)
#define CS_CLOSED ((uint8_t)4)
#define CS_DELETE ((uint8_t)8)
#define CS_STOPPED ((uint8_t)16)

struct connection {
    int id;
    uint8_t state;
    uint8_t in_state;
    uint8_t out_state;
    struct sockaddr_in address;
    struct round_buffer *in_buf;
    struct round_buffer *out_buf;
};

struct connection_manager;

int is_alive(uint8_t state);

int is_stopped(uint8_t state);

int cm_poll(struct connection_manager *cm);

int cm_connect(
        struct connection_manager *cm,
        struct sockaddr_in *addr,
        size_t in_buf_size,
        size_t out_buf_size,
        int id);

int cm_add_connection(
        struct connection_manager *cm,
        struct connection *connection,
        int fd);

struct connection *const *cm_get_connections(struct connection_manager *cm, int *count);

struct connection *make_connection(
        struct sockaddr_in *addr,
        size_t in_buf_size,
        size_t out_buf_size,
        int id);

struct connection_manager *init_manager();

struct connection_manager *init_accepting_manager(
        struct sockaddr_in *addr,
        int buf_size, int backlog);

void destroy_manager(struct connection_manager *m);

#endif
