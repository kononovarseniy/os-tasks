#include "line_list.h"

#include <stdlib.h>

struct node *make_node(struct line *line, struct node *next) {
    struct node *node = (struct node *) malloc(sizeof(struct node));
    if (node == NULL)
        return NULL;
    node->line = *line;
    node->next = next;
    return node;
}

void free_node_recursive(struct node *head) {
    while (head != NULL) {
        struct node *next = head->next;
        free(head);
        head = next;
    }
}

void init_list(struct list *list) {
    list->head = NULL;
    list->last = NULL;
}

void free_list(struct list *list) {
    free_node_recursive(list->head);
}

struct node *get_node(struct node *head, unsigned long offset) {
    while (offset-- && head != NULL)
        head = head->next;
    return head;
}

void list_add(struct list *list, struct node *node) {
    if (list->head == NULL)
        list->last = list->head = node;
    else
        list->last = list->last->next = node;
}

int add_line(struct list *list, struct line *line) {
    struct node *node = make_node(line, NULL);
    if (node == NULL)
        return 0;

    list_add(list, node);
    return 1;
}
