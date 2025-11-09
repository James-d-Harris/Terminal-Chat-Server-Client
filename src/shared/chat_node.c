#include "chat_node.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * cn_list_create
 * --------------
 * Creates an empty chat node list. We represent an empty list as NULL
 */
chat_node_list_t *cn_list_create(void) { 
    return NULL; 
}

/*
 * cn_list_free
 * ------------
 * Frees an entire linked list of chat_node_list_t entries.
 * Also closes each socket stored in each chat_node_t.
 *
 * This actively closes sockets. Only called during shutdown
 * when all client connections should be terminated.
 */
void cn_list_free(chat_node_list_t *head) {
    while (head) {
        chat_node_list_t *next_node = head->next;
        close(head->node.sock);  /* Close client's TCP socket */
        free(head);              /* Free list node wrapper */
        head = next_node;
    }
}

/*
 * cn_add
 * ------
 * Adds a new chat_node_t to the head of the linked list.
 * Returns:
 *   0 on success
 *  -1 on allocation failure
 */
int cn_add(chat_node_list_t **head, const chat_node_t *new_node) {
    chat_node_list_t *entry = calloc(1, sizeof(*entry));
    if (!entry) return -1;

    entry->node = *new_node;   /* Copy node data (name + socket) */
    entry->next = *head;       /* Insert at list front */
    *head = entry;
    return 0;
}

/*
 * cn_remove_by_sock
 * -----------------
 * Removes a client from the list based on matching socket fd.
 * Also closes that client's socket before freeing the node.
 * Returns:
 *   0 on success (node removed)
 *  -1 if no matching socket was found
 */
int cn_remove_by_sock(chat_node_list_t **head, int sock) {
    for (chat_node_list_t **current = head; *current; current = &(*current)->next) {
        if ((*current)->node.sock == sock) {
            chat_node_list_t *to_delete = *current;
            *current = to_delete->next;
            close(to_delete->node.sock);
            free(to_delete);
            return 0;
        }
    }
    return -1;
}

/*
 * cn_remove_by_name
 * -----------------
 * Removes a client from the list based on their chat username.
 * Also closes the socket and frees the node.
 */
int cn_remove_by_name(chat_node_list_t **head, const char *name) {
    for (chat_node_list_t **current = head; *current; current = &(*current)->next) {
        if (strcmp((*current)->node.name, name) == 0) {
            chat_node_list_t *to_delete = *current;
            *current = to_delete->next;
            close(to_delete->node.sock);
            free(to_delete);
            return 0;
        }
    }
    return -1;
}

/*
 * cn_find_by_name
 * ----------------
 * Searches for an existing participant by their human-readable name.
 * Returns:
 *   pointer to the chat_node_t entry, OR
 *   NULL if not found.
 *
 * Used by server JOIN validation to ensure names are unique.
 */
chat_node_t *cn_find_by_name(chat_node_list_t *head, const char *name) {
    for (; head; head = head->next) {
        if (strcmp(head->node.name, name) == 0)
            return &head->node;
    }
    return NULL;
}
