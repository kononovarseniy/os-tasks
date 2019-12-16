#include "controller.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>

static const int buffer_size = 10240;

int parse_port(const char *str, in_port_t *res) {
    char *end;
    errno = 0;
    long r = strtol(str, &end, 10);
    if (r < 0 || r > 65535)
        return -1;
    *res = (in_port_t) r;
    return 0;
}

void print_usage_and_exit() {
    fprintf(stderr, "USAGE: portfwd (server|client) <listen-port> <target-ip> <target-port>");
    exit(1);
}

int is_server;
struct sockaddr_in listen_addr;
struct sockaddr_in dst_address;

void parse_args(int argc, char *const argv[]) {
    if (argc != 5) {
        print_usage_and_exit();
    }

    is_server = 0;
    if (strcmp(argv[1], "server") == 0) {
        is_server = 1;
    } else if (strcmp(argv[1], "client") != 0) {
        fprintf(stderr, "unknown command \"%s\"\n", argv[1]);
        print_usage_and_exit();
    }

    in_port_t port;
    if (parse_port(argv[2], &port) == -1) {
        fprintf(stderr, "Invalid listening port value\n");
        print_usage_and_exit();
    }

    struct in_addr target_ip_addr;
    if (!inet_aton(argv[3], &target_ip_addr)) {
        fprintf(stderr, "Invalid target ip address\n");
        print_usage_and_exit();
    }

    in_port_t target_port;
    if (parse_port(argv[4], &target_port) == -1) {
        fprintf(stderr, "Invalid target port value\n");
        print_usage_and_exit();
    }

    struct in_addr listen_ip_addr;
    listen_ip_addr.s_addr = htonl(INADDR_ANY);

    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(port);
    listen_addr.sin_addr = listen_ip_addr;

    dst_address.sin_family = AF_INET;
    dst_address.sin_port = htons(target_port);
    dst_address.sin_addr = target_ip_addr;
}

volatile int term_signal_received = 0;
void termination_signal_handler(int sig) {
    term_signal_received = 1;
}

void setup_signals() {
    struct sigaction action;
    action.sa_handler = termination_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);

    action.sa_handler = SIG_IGN;

    sigaction(SIGPIPE, &action, NULL);
}

int main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup_signals();

    printf("Starting controller...\n");
    struct controller *controller;
    if (is_server)
        controller = start_controller(buffer_size, 0, &listen_addr, &dst_address, -1);
    else
        controller = start_controller(buffer_size, 1, &listen_addr, &dst_address, 10);
    printf("Controller started\n");
    if (controller == NULL) {
        fprintf(stderr, "Start failed\n");
        return 1;
    }

    while (!term_signal_received) {
        if (!update(controller)) {
            fprintf(stderr, "Something went wrong\n");
            break;
        }
    }

    printf("Destroying controller\n");
    destroy_controller(controller);
    return 0;
}
