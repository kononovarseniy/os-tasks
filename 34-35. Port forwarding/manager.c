#include "manager.h"

#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SOCKETS 1024
#define MAX_CONNECTIONS (MAX_SOCKETS - 1)

struct connection_manager {
    size_t buf_size;
    size_t connections_count;
    struct pollfd fds[MAX_SOCKETS];
    struct connection *connections[MAX_CONNECTIONS];
};

#define cm_conn_fd(cm, i) ((cm)->fds[1 + (i)])

int is_alive(uint8_t state) {
    return !(state & (CS_EOF | CS_CLOSED | CS_DELETE));
}

int is_stopped(uint8_t state) {
    return state & CS_STOPPED;
}

static int is_closed(uint8_t state) {
    return state & CS_CLOSED;
}

static int should_close(uint8_t state) {
    return state & (CS_EOF | CS_DELETE);
}

static int should_close_socket(struct connection *c) {
    return should_close(c->state) ||
           (!is_alive(c->in_state) && !is_alive(c->out_state));
}

static int should_delete(uint8_t state) {
    return (state & CS_DELETE) && (state & CS_CLOSED);
}

struct connection *make_connection(
        struct sockaddr_in *addr,
        size_t in_buf_size,
        size_t out_buf_size,
        int id) {
    struct round_buffer *in_buf = buf_create(in_buf_size);
    if (in_buf == NULL) {
        perror("make_connection: buf_create");
        return NULL;
    }
    struct round_buffer *out_buf = buf_create(out_buf_size);
    if (out_buf == NULL) {
        perror("make_connection: buf_create");
        buf_destroy(in_buf);
        return NULL;
    }
    struct connection *c = malloc(sizeof(struct connection));
    if (c == NULL) {
        perror("make_connection: malloc");
        buf_destroy(in_buf);
        buf_destroy(out_buf);
        return NULL;
    }

    c->id = id;
    c->state = 0;
    c->in_state = 0;
    c->out_state = 0;
    c->address = *addr;
    c->in_buf = in_buf;
    c->out_buf = out_buf;

    return c;
}

static void free_connection(struct connection *c) {
    buf_destroy(c->in_buf);
    buf_destroy(c->out_buf);
    free(c);
}

int cm_add_connection(
        struct connection_manager *cm,
        struct connection *connection,
        int fd) {
    if (cm->connections_count == MAX_CONNECTIONS)
        return -1;

    int ind = cm->connections_count++;
    cm->connections[ind] = connection;

    struct pollfd *pfd = &cm_conn_fd(cm, ind);
    pfd->fd = fd;
    pfd->events = POLLIN;

    return ind;
}

int cm_connect(
        struct connection_manager *cm,
        struct sockaddr_in *addr,
        size_t in_buf_size,
        size_t out_buf_size,
        int id) {
    if (cm->connections_count == MAX_CONNECTIONS) {
        fprintf(stderr, "cm_connect: too many connections");
        return 0;
    }
    struct connection *c = make_connection(addr, in_buf_size, out_buf_size, id);
    if (c == NULL) {
        fprintf(stderr, "cm_connect: failed\n");
        return 0;
    }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        perror("cm_connect: socket");
        free_connection(c);
        return 0;
    }

    if (connect(s, (struct sockaddr *) addr, sizeof(struct sockaddr_in)) == -1) {
        perror("cm_connect: connect");
        if (close(s))
            perror("close");
        free_connection(c);
        return 0;
    }

    cm_add_connection(cm, c, s);

    return 1;
}

static int cm_get_connection_fd(struct connection_manager *m, int ind) {
    int fd = cm_conn_fd(m, ind).fd;
    if (fd < 0)
        fd = ~fd;
    return fd;
}

struct connection *const*cm_get_connections(struct connection_manager *cm, int *count) {
    *count = cm->connections_count;
    return cm->connections;
}

struct connection_manager *init_manager() {
    struct connection_manager *m = malloc(sizeof(struct connection_manager));
    if (m == NULL) {
        perror("malloc");
        return NULL;
    }
    memset(m, 0, sizeof(struct connection_manager));

    m->connections_count = 0;
    m->fds[0].fd = -1;

    return m;
}

struct connection_manager *init_accepting_manager(
        struct sockaddr_in *addr,
        int buf_size, int backlog) {
    struct connection_manager *m = init_manager();
    if (m == NULL)
        return NULL;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        goto abort;
    }

    if (bind(sock, (struct sockaddr *) addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        goto abort;
    }

    if (listen(sock, backlog) == -1) {
        perror("listen");
        goto abort;
    }

    m->buf_size = buf_size;
    m->fds[0].fd = sock;
    m->fds[0].events = POLLIN;

    return m;
    abort:
    fprintf(stderr, "Closing socket\n");

    if (sock != -1 && close(sock) == -1)
        perror("close");

    destroy_manager(m);
    return NULL;
}

void destroy_manager(struct connection_manager *m) {
    if (m->fds[0].fd != -1) {
        if (close(m->fds[0].fd) == -1)
            perror("close listening socket");
    }

    for (size_t i = 0; i < m->connections_count; i++) {
        struct connection *c = m->connections[i];

        int fd = cm_get_connection_fd(m, i);
        uint8_t state = c->state;

        free_connection(c);

        if (!is_closed(state) && close(fd) == -1)
            perror("close");
    }
    free(m);
}

#define CAUSE_NONE 0
#define CAUSE_EOF 1
#define CAUSE_ERROR 2

static int get_rw_error_cause(int res, int err) {
    if (res == -1) {
        switch (err) {
            case EAGAIN:
            case EINTR:
                // Ignore these errors.
                break;
            case ECONNRESET:
            case EPIPE:
                return CAUSE_EOF;
            default:
                return CAUSE_ERROR;
        }
    } else if (res == 0) {
        return CAUSE_EOF;
    }
    return CAUSE_NONE;
}

static void clear_pollfd_flags(struct pollfd *fd, short mask) {
    if ((fd->events &= ~mask) == 0 && fd->fd >= 0) {
        // We don't want to receive POLLHUP after we shutdown reading from socket.
        fd->fd = ~fd->fd; // Negative fd values are ignored by poll
    }
}

static void set_pollfd_flags(struct pollfd *fd, short mask) {
    if (!mask)
        return;

    if (fd->fd < 0)
        fd->fd = ~fd->fd;

    fd->events |= mask;
}

static void receive(struct pollfd *fd, struct connection *conn) {
    if (!is_alive(conn->in_state) || is_stopped(conn->state))
        return;
    struct round_buffer *buf = conn->in_buf;

    if ((fd->revents & POLLIN) && !buf_full(buf)) {
        ssize_t res = buf_read(fd->fd, buf);
        switch (get_rw_error_cause(res, errno)) {
            case CAUSE_ERROR:
                perror("read");
                /* FALLTHROUGH */
            case CAUSE_EOF:
                conn->in_state |= CS_EOF;
        }
    }

    if (buf_full(buf) || !is_alive(conn->in_state))
        clear_pollfd_flags(fd, POLLIN);
    else
        set_pollfd_flags(fd, POLLIN);
}

static void transmit(struct pollfd *fd, struct connection *conn) {
    if (!is_alive(conn->out_state) || is_stopped(conn->state))
        return;

    struct round_buffer *buf = conn->out_buf;

    if ((fd->revents & POLLOUT) && !buf_empty(buf)) {
        ssize_t res = buf_write(fd->fd, buf);
        switch (get_rw_error_cause(res, errno)) {
            case CAUSE_ERROR:
                perror("write");
                /* FALLTHROUGH */
            case CAUSE_EOF:
                conn->out_state |= CS_EOF;
        }
    }

    if (buf_empty(buf) || !is_alive(conn->out_state))
        clear_pollfd_flags(fd, POLLOUT);
    else
        set_pollfd_flags(fd, POLLOUT);
}

static void try_shutdown(int fd, int dir) {
    if (fd < 0)
        fd = ~fd;

    if (shutdown(fd, dir) == -1 && errno != ENOTCONN)
        // ENOTCONN appears when remote host closes both directions
        perror("shutdown");
}

static void accept_connection(struct connection_manager *cm) {
    if (cm->connections_count == MAX_CONNECTIONS)
        return;

    int listening_socket = cm->fds[0].fd;
    socklen_t len = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    int socket = accept(listening_socket, (struct sockaddr *) &addr, &len);
    if (socket == -1) {
        if (errno == EINTR || errno == EAGAIN)
            return;
        perror("accept");
    }

    struct connection *c = make_connection(&addr, cm->buf_size, cm->buf_size, -1);
    if (c == NULL) {
        fprintf(stderr, "make_connection failed\n");
        goto abort;
    }
    c->state |= CS_NEW;

    if (cm_add_connection(cm, c, socket) == -1) {
        fprintf(stderr, "cm_add_connection failed\n");
        goto abort;
    }

    return;
    abort:
    fprintf(stderr, "Closing socket %s:%hu...\n",
            inet_ntoa(c->address.sin_addr),
            ntohs(c->address.sin_port));

    if (c != NULL)
        free_connection(c);

    if (close(socket))
        perror("close");
}

static void close_sockets(struct connection_manager *cm) {
    for (size_t i = 0; i < cm->connections_count; i++) {
        struct connection *conn = cm->connections[i];
        if (is_closed(conn->state))
            continue;

        int fd = cm_get_connection_fd(cm, i);

        if (should_close_socket(conn)) {
            cm_conn_fd(cm, i).fd = -1;

            fprintf(stderr, "Closing socket %s:%hu...\n",
                    inet_ntoa(conn->address.sin_addr),
                    ntohs(conn->address.sin_port));

            if (close(fd) == -1)
                perror("close");

            conn->state |= CS_CLOSED;
            conn->in_state |= CS_CLOSED;
            conn->out_state |= CS_CLOSED;
        } else {
            // Shutdown direction if required
            if (should_close(conn->in_state) && !is_closed(conn->in_state)) {
                try_shutdown(fd, SHUT_RD);
                conn->in_state |= CS_CLOSED;
            }
            if (should_close(conn->out_state) && !is_closed(conn->out_state)) {
                try_shutdown(fd, SHUT_WR);
                conn->out_state |= CS_CLOSED;
            }
        }
    }
}

static void shrink(struct connection_manager *cm) {
    int i = 0;
    for (int j = 0; j < cm->connections_count; j++) {
        if (should_delete(cm->connections[j]->state)) {
            free_connection(cm->connections[j]);
        } else {
            if (i != j) {
                cm->connections[i] = cm->connections[j];
                cm_conn_fd(cm, i) = cm_conn_fd(cm, j);
            }
            i++;
        }
    }
    cm->connections_count = i;
}

int cm_poll(struct connection_manager *cm) {
    close_sockets(cm);
    shrink(cm);

    int connections_count = cm->connections_count;
    for (int i = 0; i < connections_count; i++) {
        struct connection *conn = cm->connections[i];
        if (!buf_empty(conn->out_buf)) {
            if (!is_alive(conn->out_state)) {
                fprintf(stderr, "cm_poll: WARNING: attempt to write to the closed connection (id: %d) detected\n",
                        conn->id);
                continue;
            }
            struct pollfd *fd = &cm_conn_fd(cm, i);
            set_pollfd_flags(fd, POLLOUT);
        }
    }

    nfds_t nfds = 1 + connections_count;
    int cnt = -1;
    while (cnt < 0) {
        cnt = poll(cm->fds, nfds, -1);
        if (cnt == -1) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("poll");
            return -1;
        }
    }
    if (cm->fds[0].revents & POLLIN)
        accept_connection(cm);

    // Transmit/receive data and set connection state on eof/error
    // (Ignore sockets accepted on previous step)
    for (int i = 0; i < connections_count; i++) {
        struct connection *conn = cm->connections[i];
        struct pollfd *fd = &cm_conn_fd(cm, i);
        if (is_alive(conn->state)) {
            receive(fd, conn);
            transmit(fd, conn);
        }
    }

    close_sockets(cm);
    shrink(cm);

    return 0;
}
