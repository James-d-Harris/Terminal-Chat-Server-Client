#define DBG
#include "dbg.h"
#include "client_handler.h"
#include "main.h"
#include "../shared/message.h"
#include "../shared/chat_node.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Convenience wrappers to send specific server->client indications.
 * These keep call sites short and make intent obvious.
 */
static int send_joining(int client_socket_fd, const char *joining_name) {
    return msg_send(client_socket_fd, MSG_JOINING, joining_name, NULL);
}
static int send_left(int client_socket_fd, const char *leaving_name) {
    return msg_send(client_socket_fd, MSG_LEFT, leaving_name, NULL);
}
static int send_deliver(int client_socket_fd, const char *from_name, const char *note_text) {
    return msg_send(client_socket_fd, MSG_DELIVER, from_name, note_text);
}
static int send_bye(int client_socket_fd) {
    return msg_send(client_socket_fd, MSG_BYE, NULL, "Server shutting down");
}

/*
 * talk_to_client
 * --------------
 * Per-connection thread entry. Handles the entire lifetime of a single client socket:
 *   - Receives length-prefixed messages (see message.c) from one client.
 *   - Validates and processes JOIN / NOTE / LEAVE / SHUTDOWN / SHUTDOWN_ALL.
 *   - Maintains global membership list g_clients under g_clients_mx.
 *   - Broadcasts JOINING/LEFT/DELIVER/BYE events to other clients.
 *
 * Concurrency & correctness notes:
 *   - We never hold g_clients_mx while sending on sockets (to avoid blocking all
 *     other clients if one slow/closed socket stalls). Instead, we snapshot the list
 *     of destination sockets under the mutex, release the lock, then send.
 *   - We duplicate the client's name (joined_client_name) after JOIN so it's valid
 *     even if message buffers are freed.
 */
void *talk_to_client(void *arg) {
    int client_socket_fd = (int)(intptr_t)arg;     /* Connected socket for this client thread */
    char *joined_client_name = NULL;               /* malloc'd copy of the client's name after JOIN */
    int   has_joined = 0;                          /* Whether this client has successfully JOINed */

    for (;;) {
        /* Receive a single framed message from this client. */
        msg_type_t incoming_type;
        char *incoming_name = NULL;                /* If present in frame; owned until msg_free */
        char *incoming_text = NULL;                /* If present in frame; owned until msg_free */

        if (msg_recv(client_socket_fd, &incoming_type, &incoming_name, &incoming_text) != 0) {
            /* Socket closed or protocol error; exit the loop and clean up. */
            debug("client socket %d closed or bad frame\n", client_socket_fd);
            break;
        }

        switch (incoming_type) {

        case MSG_JOIN:
            /*
             * A client wants to JOIN the chat with a logical name (incoming_name).
             * We accept only if:
             *   - it hasn't joined yet, and
             *   - a non-empty name is provided, and
             *   - the name is not already taken in g_clients.
             *
             * On success:
             *   - Add to g_clients.
             *   - Remember joined_client_name.
             *   - Notify all other clients via MSG_JOINING (outside the lock).
             */
            if (!has_joined && incoming_name && *incoming_name) {
                debug("JOIN from %s\n", incoming_name);

                /* Dynamic array of sockets to notify after releasing the lock. */
                int   *notify_sockets = NULL;
                size_t notify_count = 0, notify_cap = 0;

                pthread_mutex_lock(&g_clients_mx);
                if (!cn_find_by_name(g_clients, incoming_name)) {
                    /* Insert new member into the global list. */
                    chat_node_t new_member = (chat_node_t){0};
                    snprintf(new_member.name, sizeof(new_member.name), "%s", incoming_name);
                    new_member.sock = client_socket_fd;
                    cn_add(&g_clients, &new_member);

                    has_joined = 1;
                    joined_client_name = strdup(incoming_name);

                    /* Snapshot recipients (all other sockets) for MSG_JOINING. */
                    for (chat_node_list_t *it = g_clients; it; it = it->next) {
                        if (it->node.sock == client_socket_fd) continue; /* do not notify the joiner */
                        if (notify_count == notify_cap) {
                            notify_cap = notify_cap ? notify_cap * 2 : 8;
                            notify_sockets = realloc(notify_sockets, notify_cap * sizeof(int));
                        }
                        notify_sockets[notify_count++] = it->node.sock;
                    }
                }
                pthread_mutex_unlock(&g_clients_mx);

                /* Perform network I/O without holding the mutex. */
                for (size_t i = 0; i < notify_count; i++) {
                    msg_send(notify_sockets[i], MSG_JOINING, joined_client_name, NULL);
                }
                free(notify_sockets);
            }
            break;

        case MSG_NOTE:
            /*
             * Forward a NOTE (incoming_text) from this client to all other participants.
             * The sender must have joined already. The payload is delivered as MSG_DELIVER
             * with sender's name (joined_client_name).
             */
            if (has_joined && incoming_text) {
                debug("NOTE from %s: %s\n", joined_client_name, incoming_text);

                int   *recipient_sockets = NULL;
                size_t recipient_count = 0, recipient_cap = 0;

                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it = g_clients; it; it = it->next) {
                    if (it->node.sock == client_socket_fd) continue; /* do not echo to originator */
                    if (recipient_count == recipient_cap) {
                        recipient_cap = recipient_cap ? recipient_cap * 2 : 8;
                        recipient_sockets = realloc(recipient_sockets, recipient_cap * sizeof(int));
                    }
                    recipient_sockets[recipient_count++] = it->node.sock;
                }
                pthread_mutex_unlock(&g_clients_mx);

                for (size_t i = 0; i < recipient_count; i++) {
                    send_deliver(recipient_sockets[i], joined_client_name, incoming_text);
                }
                free(recipient_sockets);
            }
            break;

        case MSG_LEAVE:
        case MSG_SHUTDOWN:
            /*
             * The client is leaving voluntarily (LEAVE) or shutting down this client only (SHUTDOWN).
             * If they had joined, we:
             *   - Snapshot all other sockets,
             *   - Remove the client from g_clients,
             *   - Broadcast MSG_LEFT to everyone else,
             *   - Mark as not joined and end the handler thread.
             */
            if (has_joined) {
                debug("LEAVE/SHUTDOWN from %s\n", joined_client_name ? joined_client_name : "(unknown)");

                int   *notify_sockets = NULL;
                size_t notify_count = 0, notify_cap = 0;

                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it = g_clients; it; it = it->next) {
                    if (it->node.sock == client_socket_fd) continue; /* do not notify the leaver */
                    if (notify_count == notify_cap) {
                        notify_cap = notify_cap ? notify_cap * 2 : 8;
                        notify_sockets = realloc(notify_sockets, notify_cap * sizeof(int));
                    }
                    notify_sockets[notify_count++] = it->node.sock;
                }
                cn_remove_by_sock(&g_clients, client_socket_fd);
                pthread_mutex_unlock(&g_clients_mx);

                for (size_t i = 0; i < notify_count; i++) {
                    send_left(notify_sockets[i], joined_client_name);
                }
                free(notify_sockets);

                has_joined = 0;
            }
            /* Free the just-received message buffers before exiting the thread. */
            msg_free(incoming_name, incoming_text);
            goto cleanup_and_exit;

        case MSG_SHUTDOWN_ALL:
            /*
             * Global shutdown request. ONLY allowed from a participant.
             *   - Sets the global flag (observed by accept loop/main).
             *   - Broadcasts MSG_BYE to everyone so clients terminate.
             * The server process will shortly shut down its accept loop.
             */
            if (has_joined) {
                g_shutdown_all = 1;
                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it = g_clients; it; it = it->next) {
                    send_bye(it->node.sock);
                }
                pthread_mutex_unlock(&g_clients_mx);
            }
            msg_free(incoming_name, incoming_text);
            goto cleanup_and_exit;

        default:
            /* Unknown / unsupported message type: ignore. */
            break;
        }

        /* Release message buffers for this iteration. */
        msg_free(incoming_name, incoming_text);
    }

cleanup_and_exit:
    /* Free per-thread resources and close the socket. */
    if (joined_client_name) free(joined_client_name);
    close(client_socket_fd);
    return NULL;
}
