#include "receiver_handler.h"
#include "../shared/message.h"
#include "text_color.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Dispatch one message that the client received from the server.
 * - MSG_DELIVER: print as "Name: note"
 * - MSG_JOINING: print notice that someone joined (not this client)
 * - MSG_LEFT:    print notice that someone left
 * - MSG_BYE:     server asks everyone to shut down (receiver will break loop)
 *
 * Colors come from text_color.h; fall back to plain text if those macros are no-ops.
 */
void  dispatch_server_message(int t, const char *name, const char *text) {
    switch ((msg_type_t)t) {
    case MSG_DELIVER:
        if (name && text) printf(NOTE_COLOR "%s: %s" RESET_COLOR "\n", name, text);
        break;
    case MSG_JOINING:
        if (name) printf(JOINED_COLOR "[info] %s joined" RESET_COLOR "\n", name);
        break;
    case MSG_LEFT:
        if (name) printf(LEFT_COLOR "[info] %s left" RESET_COLOR "\n", name);
        break;
    case MSG_BYE:
        if (text) printf("[server] %s\n", text);
        break;
    default:
        /* Unknown message types are silently ignored. */
        break;
    }
    fflush(stdout);
}

/*
 * Receiver thread:
 * - Blocks on msg_recv() to read framed messages.
 * - For each message, hands it to dispatch_server_message().
 * - Exits when MSG_BYE is received or when the connection is closed.
 */
void *receiver_thread(void *arg) {
    int server_socket_fd = (int)(intptr_t)arg;

    for (;;) {
        msg_type_t received_type;
        char *received_name = NULL;
        char *received_text = NULL;

        if (msg_recv(server_socket_fd, &received_type, &received_name, &received_text) != 0)
            break;

        dispatch_server_message((int)received_type, received_name, received_text);
        msg_free(received_name, received_text);

        if (received_type == MSG_BYE) break;
    }

    close(server_socket_fd);
    return NULL;
}
