#include "controller.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define TUNNEL_BUF_SIZE_MULTIPLIER 10
#define MAX_STACK_SIZE (MAX_CONNECTIONS - 1)
#define QUEUE_SIZE (MAX_CONNECTIONS * 2)

struct id_stack {
    int stack[MAX_STACK_SIZE];
    int cnt;
};

struct controller {
    struct connection_manager *manager;
    struct pump *pump;
    struct id_stack free_ids;
    struct sockaddr_in address;
    int accepting;
    size_t buf_size;
};

static void id_stack_init(struct id_stack *s) {
    for (int i = 0; i < MAX_STACK_SIZE; i++)
        s->stack[i] = MAX_STACK_SIZE - i;
    s->cnt = MAX_CONNECTIONS - 1;
}

static int id_stack_empty(struct id_stack *s) {
    return s->cnt == 0;
}

static void id_stack_push(struct id_stack *s, int id) {
    if (s->cnt == MAX_STACK_SIZE) {
        fprintf(stderr, "controller: free id stack overflow\n");
        return;
    }
    s->stack[s->cnt++] = id;
}

static int id_stack_pop(struct id_stack *s) {
    if (s->cnt == 0) {
        fprintf(stderr, "controller: free id stack underflow\n");
        return -1;
    }
    return s->stack[--s->cnt];
}

static void sort_connections(
        struct id_stack *stack,
        struct connection *const *connections,
        struct connection **sorted,
        size_t count) {
    memset(sorted, 0, sizeof(void *) * MAX_CONNECTIONS);
    while (count--) {
        struct connection *c = *connections++;
        if (c->id > MAX_CONNECTIONS || c->id < 0) {
            if (id_stack_empty(stack))
                continue;
            c->id = id_stack_pop(stack);
        }
        sorted[c->id] = c;
    }
}

static int is_new_connection(struct connection *c) {
    return c->state & CS_NEW;
}

static int should_close_connection(struct connection *c) {
    return buf_empty(c->in_buf) && buf_empty(c->out_buf) && (c->state & CS_CLOSED);
}

static int should_close_in(struct connection *c) {
    return buf_empty(c->in_buf) && (c->in_state & CS_CLOSED);
}

static int should_close_out(struct connection *c) {
    return buf_empty(c->out_buf) && (c->out_state & CS_CLOSED);
}

#define STATE_OK 1
#define STATE_AGAIN 2
#define STATE_SHUTDOWN 3

static int handle_state_changes(struct controller *controller, struct connection **connections) {
    struct connection *tunnel = connections[0];
    if (tunnel->state & CS_CLOSED || tunnel->in_state & CS_CLOSED || tunnel->out_state & CS_CLOSED) {
        return STATE_SHUTDOWN;
    }
    for (int i = 1; i < 256; i++) {
        struct connection *c = connections[i];
        if (c == NULL)
            continue;
        if (is_new_connection(c)) {
            if (!send_command(controller->pump, CMD_NEW, c->id))
                return STATE_AGAIN;
            c->state &= ~CS_NEW;
            c->state |= CS_STOPPED;
            printf("New connection accepted (id: %d)\n", c->id);
        }
        if (should_close_connection(c)) {
            if (!send_command(controller->pump, CMD_CLOSE, c->id))
                return STATE_AGAIN;
            c->state |= CS_DELETE;
            if (controller->accepting)
                id_stack_push(&controller->free_ids, c->id);
            connections[i] = NULL; // Ignore the connection on next stages.
            printf("Connection closed by socket (id: %d)\n", c->id);
        } else {
            if (!(c->in_state & CS_DELETE) && should_close_in(c)) {
                if (!send_command(controller->pump, CMD_CLOSE_SRC_TO_DST, c->id))
                    return STATE_AGAIN;
                c->in_state |= CS_DELETE;
                printf("(source -> destination) closed by socket (id: %d)\n", c->id);
            }
            if (!(c->out_state & CS_DELETE) && should_close_out(c)) {
                if (!send_command(controller->pump, CMD_CLOSE_DST_TO_SRC, c->id))
                    return STATE_AGAIN;
                c->out_state |= CS_DELETE;
                printf("(destination -> source) closed by socket (id: %d)\n", c->id);
            }
        }
    }
    return STATE_OK;
}

static void on_command(void *self, uint8_t cmd, uint8_t arg) {
    struct controller *c = self;

    if (cmd == CMD_NEW) {
        int count = 0;
        struct connection *const *cs = cm_get_connections(c->manager, &count);
        for (int i = 0; i < count; ++i) {
            if (cs[i]->id == arg) {
                // TODO: shutdown or just ignore
                return;
            }
        }
        if (cm_connect(c->manager, &c->address, c->buf_size, c->buf_size, arg)) {
            if (!send_command(c->pump, CMD_ACK, arg)) {
                // This should newer happen if queue size is enough.
                fprintf(stderr, "controller: on_command: WARNING command queue overflow\n");
                // TODO: shutdown
            }
            printf("New connection established\n");
        } else {
            // We can't send commands directly, because buffer may be full.
            if (!send_command(c->pump, CMD_CLOSE, arg)) {
                // This should newer happen if queue size is enough.
                fprintf(stderr, "controller: on_command: WARNING command queue overflow\n");
                // TODO: shutdown
            }
            return;
        }
    } else if (cmd == CMD_CLOSE || // Commands that need to find connection by id
               cmd == CMD_CLOSE_SRC_TO_DST ||
               cmd == CMD_CLOSE_DST_TO_SRC ||
               cmd == CMD_ACK) {
        int count = 0;
        struct connection *const *cs = cm_get_connections(c->manager, &count);
        for (int i = 0; i < count; ++i) {
            if (cs[i]->id == arg) {
                if (cmd == CMD_CLOSE) {
                    cs[i]->state |= CS_DELETE;
                    if (c->accepting)
                        id_stack_push(&c->free_ids, cs[i]->id);
                    printf("Connection closed by command (id: %d)\n", arg);
                } else if (cmd == CMD_CLOSE_SRC_TO_DST) {
                    cs[i]->out_state |= CS_DELETE;
                    printf("(source -> destination) closed by command (id: %d)\n", arg);
                } else if (cmd == CMD_CLOSE_DST_TO_SRC) {
                    cs[i]->in_state |= CS_DELETE;
                    printf("(destination -> source) closed by command (id: %d)\n", arg);
                } else if (cmd == CMD_ACK) {
                    cs[i]->state &= ~CS_STOPPED;
                }
                return;
            }
        }
    }
}

int accept_tunnel_connection(struct controller *controller, struct sockaddr_in *addr, size_t buf_size) {
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == -1) {
        perror("accept_tunnel_connection: socket");
        return 0;
    }
    if (bind(ls, (struct sockaddr *) addr, sizeof(struct sockaddr_in))) {
        perror("accept_tunnel_connection: bind");
        goto fail;
    }
    if (listen(ls, 1)) {
        perror("accept_tunnel_connection: listen");
        goto fail;
    }
    struct sockaddr_in tunnel_addr;
    socklen_t len = sizeof(tunnel_addr);
    int s = accept(ls, (struct sockaddr *) &tunnel_addr, &len);
    if (s == -1) {
        perror("accept_tunnel_connection: accept");
        goto fail;
    }

    struct connection *c = make_connection(&tunnel_addr, buf_size, buf_size, 0);
    if (c == NULL) {
        fprintf(stderr, "accept_tunnel_connection: make_connection failed\n");
        goto fail;
    }

    cm_add_connection(controller->manager, c, s);

    if (close(ls))
        perror("accept_tunnel_connection: close");
    return 1;

    fail:
    if (close(ls))
        perror("accept_tunnel_connection: close");
    return 0;
}

struct controller *
start_controller(size_t buf_size,
                 int accepting,
                 struct sockaddr_in *listen_addr,
                 struct sockaddr_in *addr,
                 int backlog) {
    struct controller *c = malloc(sizeof(struct controller));
    if (c == NULL) {
        perror("start_controller: malloc");
        return NULL;
    }

    struct pump *pump = make_pump(QUEUE_SIZE, on_command, c);
    if (pump == NULL) {
        perror("start_controller: make_pump");
        goto pump_failed;
    }

    struct connection_manager *manager;
    if (accepting)
        manager = init_accepting_manager(listen_addr, buf_size, backlog);
    else
        manager = init_manager();

    if (manager == NULL) {
        fprintf(stderr, "start_controller: Unable to create connection manager\n");
        goto manager_failed;
    }

    c->pump = pump;
    c->manager = manager;
    c->address = *addr;
    c->buf_size = buf_size;
    c->accepting = accepting;
    id_stack_init(&c->free_ids);

    size_t bs = buf_size * TUNNEL_BUF_SIZE_MULTIPLIER;
    if (accepting) {
        if (!cm_connect(manager, addr, bs, bs, 0)) {
            fprintf(stderr, "start_controller: Cannot connect to server\n");
            goto tunnel_failed;
        }
    } else {
        if (!accept_tunnel_connection(c, listen_addr, bs)) {
            fprintf(stderr, "start_controller: Cannot accept client connection\n");
            goto tunnel_failed;
        }
    }

    return c;

    tunnel_failed:
    destroy_manager(manager);

    manager_failed:
    free_pump(pump);

    pump_failed:
    free(c);

    return NULL;

}

void shutdown_controller(struct controller *c) {
    cm_shutdown(c->manager);
}

void destroy_controller(struct controller *c) {
    if (c == NULL)
        return;

    free_pump(c->pump);
    destroy_manager(c->manager);
    free(c);
}

int update(struct controller *controller) {
    int cause = cm_poll(controller->manager);
    if (cause != CLOSE_CAUSE_NONE)
        return cause;

    int count = 0;
    struct connection *const *manager_connections = cm_get_connections(controller->manager, &count);
    struct connection *connections[MAX_CONNECTIONS];
    sort_connections(&controller->free_ids, manager_connections, connections, count);

    if (count == 0) {
        fprintf(stderr, "controller: no connections\n");
        return CLOSE_CAUSE_NONE;
    }
    if (connections[0] == NULL) {
        fprintf(stderr, "controller: no tunnel\n");
        return CLOSE_CAUSE_NONE;
    }

    int state = handle_state_changes(controller, connections);
    if (state == STATE_AGAIN) return CLOSE_CAUSE_NONE;
    if (state == STATE_SHUTDOWN) return CLOSE_CAUSE_ERROR;

    pump_transfer(controller->pump, connections);

    return CLOSE_CAUSE_NONE;
}
