#include "chat_node.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

chat_node_list_t *cn_list_create(void) { return NULL; }

void cn_list_free(chat_node_list_t *head) {
    while (head) { chat_node_list_t *n = head->next; close(head->node.sock); free(head); head = n; }
}

int cn_add(chat_node_list_t **head, const chat_node_t *n) {
    chat_node_list_t *e = calloc(1, sizeof(*e)); if (!e) return -1;
    e->node = *n; e->next = *head; *head = e; return 0;
}

int cn_remove_by_sock(chat_node_list_t **head, int sock) {
    for (chat_node_list_t **pp=head; *pp; pp=&(*pp)->next) {
        if ((*pp)->node.sock == sock) { chat_node_list_t *del=*pp; *pp=del->next; close(del->node.sock); free(del); return 0; }
    }
    return -1;
}

int cn_remove_by_name(chat_node_list_t **head, const char *name) {
    for (chat_node_list_t **pp=head; *pp; pp=&(*pp)->next) {
        if (strcmp((*pp)->node.name, name)==0) { chat_node_list_t *del=*pp; *pp=del->next; close(del->node.sock); free(del); return 0; }
    }
    return -1;
}

chat_node_t *cn_find_by_name(chat_node_list_t *head, const char *name) {
    for (; head; head=head->next) if (strcmp(head->node.name, name)==0) return &head->node;
    return NULL;
}
