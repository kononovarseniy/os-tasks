#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h> // Man recomends to include this header alongside with socket.h altough it is not required
#include <sys/socket.h> // socket, bind
#include <netinet/in.h> // sockaddr_in, in_port_t, in_addr
#include <arpa/inet.h> // inet_aton, inet_ntoa
#include <unistd.h> // close
#include <stdlib.h>
#include <stdio.h> // perror
#include <string.h> // memset

#define BUF_SIZE 256
#define CLIENTS_COUNT 510
#define REPEATS_COUNT 10000

struct sockaddr_in dst_address;

int parse_port(const char *str, in_port_t *res) {
    char *end;
    errno = 0;
    long r = strtol(str, &end, 10);
    if (r < 0 || r > 65535)
        return -1;
    *res = (in_port_t) r;
    return 0;
}

void print_usage_and_exit(const char *name) {
    fprintf(stderr, "USAGE %s <target-ip> <target-port>\n", name);
    exit(1);
}

void parse_args(int argc, char *const argv[]) {
    if (argc != 3) {
        print_usage_and_exit(argv[0]);
    }

    struct in_addr target_ip_addr;
    if (!inet_aton(argv[1], &target_ip_addr)) {
        fprintf(stderr, "Invalid target ip address\n");
        print_usage_and_exit(argv[0]);
    }

    in_port_t target_port;
    if (parse_port(argv[2], &target_port) == -1) {
        fprintf(stderr, "Invalid target port value\n");
        print_usage_and_exit(argv[0]);
    }

    dst_address.sin_family = AF_INET;
    dst_address.sin_port = htons(target_port);
    dst_address.sin_addr = target_ip_addr;
}

int child_main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("sock");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&dst_address, sizeof(dst_address)) == -1) {
        perror("connect");
        if (close(sock) == -1)
            perror("close");
        return 1;
    }
    
    char buf[BUF_SIZE], recv_buf[BUF_SIZE];
    for (int j = 0; j < BUF_SIZE; j++)
        buf[j] = j;

    for (int i = 0; i < REPEATS_COUNT; i++) {
        char *ptr = buf;
        size_t len = sizeof(buf);
        while (len > 0) {
            ssize_t res = write(sock, ptr, len);
            if (res == -1) {
                perror("write");
                goto end;
            }
            len -= res;
        }
        ptr = recv_buf;
        len = sizeof(recv_buf);
        while (len > 0) {
            ssize_t res = read(sock, ptr, len);
            if (res == -1) {
                perror("read");
                goto end;
            }
            len -= res;
        }
        for (int i = 0; i < BUF_SIZE; i++) {
            if (buf[i] != recv_buf[i]) {
                fprintf(stderr, "ERROR\n");
                goto end;
            }
            buf[i]++;
        }
    }
end:
    if (close(sock) == -1)
        perror("close");
    return 0;
}

int main(int argc, char *const argv[]) {
    parse_args(argc, argv); 

    pid_t pids[CLIENTS_COUNT];
    for (int i = 0; i < CLIENTS_COUNT; i++) {
        pid_t pid = pids[i] = fork();
        if (pid == -1) {
            perror("fork");
        } else if (pid == 0) {
            return child_main();
        }
    }
    for (int i = 0; i < CLIENTS_COUNT; i++) {
        if (pids[i] > 0) {
            for (;;) {
                if(waitpid(pids[i], NULL, 0) == -1) {
                    if (errno == EINTR)
                        continue;
                    perror("waitpid");
                }
                break;
            }
        }
    }
    return 0;
}
