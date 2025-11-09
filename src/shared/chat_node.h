#pragma once
#include <netinet/in.h>

typedef struct {
    char name[64];
    int  sock;
    struct sockaddr_in addr;
} chat_node_t;

typedef struct chat_node_list {
    chat_node_t            node;
    struct chat_node_list *next;
} chat_node_list_t;

// helpers
chat_node_list_t *cn_list_create(void);
void cn_list_free(chat_node_list_t *head);
int  cn_add(chat_node_list_t **head, const chat_node_t *n);
int  cn_remove_by_sock(chat_node_list_t **head, int sock);
int  cn_remove_by_name(chat_node_list_t **head, const char *name);
chat_node_t *cn_find_by_name(chat_node_list_t *head, const char *name);
