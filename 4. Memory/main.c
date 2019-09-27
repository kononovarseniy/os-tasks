#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct list_node {
    struct list_node *next;
    char *data;
};

struct list_node *head = NULL;
struct list_node *last = NULL;

#define BUF_SIZE 255

const char *read_line() {
    static char buf[BUF_SIZE];
    if (fgets(buf, BUF_SIZE, stdin) == NULL)
        return ".";
    return buf;
}

struct list_node *make_node(char *data) {
    struct list_node *node = (struct list_node *) malloc(sizeof(struct list_node));
    if (node == NULL)
        return NULL;
    node->data = data;
    node->next = NULL;
    return node;
}

void free_list(struct list_node *node) {
    if (node == NULL)
        return;
    free_list(node->next);
    free(node->data);
    free(node);
}

int add_line(const char *line) {
    char *c = strdup(line);
    if (c == NULL)
        return 0;

    struct list_node *node = make_node(c);
    if (node == NULL) {
        free(c);
        return 0;
    }

    if (head == NULL)
        head = last = node;
    else
        last = last->next = node;
    return 1;
}

int main()
{
    const char *line;
    int success = 1;
    while (*(line = read_line()) != '.') {
        if (!add_line(line)) {
            perror("Unable to add line");
            success = 0;
            break;
        }
    }

    if (success) {
        for (struct list_node *node = head; node; node = node->next) {
            printf("%s", node->data);
        }
    }

    free_list(head);

    return 0;
}
