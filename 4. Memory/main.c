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

char *copy_string(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = (char *) malloc(sizeof(char) * len);
    if (copy == NULL)
    {
        perror("Unable to allocate memory for a string");
        exit(1);
    }
    memcpy(copy, str, len);
    return copy;
}

#define BUF_SIZE 255
char *read_line() {
    static char buf[BUF_SIZE];
    if (fgets(buf, BUF_SIZE, stdin) == NULL)
        return copy_string(".");
    return copy_string(buf);
}

struct list_node *make_node(char *data) {
    struct list_node *node = (struct list_node *) malloc(sizeof(struct list_node));
    if (node == NULL) {
        perror("Unnable to allocate memory fpo a list node");
        exit(1);
    }
    node->data = data;
    node->next = NULL;
    return node;
}

int main()
{
    char *line;
    while (*(line = read_line()) != '.') {
        struct list_node *node = make_node(line);
        if (head == NULL)
            head = last = node;
        else
            last = last->next = node;
    }
    
    for (struct list_node *node = head; node; node = node->next)
        printf("%s", node->data);
        
    return 0;
}
