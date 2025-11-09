#include "receiver_handler.h"
#include "../shared/message.h"
#include "text_color.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
        break;
    }
    fflush(stdout);
}

void *receiver_thread(void *arg) {
    int sock = (int)(intptr_t)arg;
    for (;;) {
        msg_type_t t; char *name=NULL, *text=NULL;
        if (msg_recv(sock, &t, &name, &text) != 0) break;
        dispatch_server_message((int)t, name, text);
        msg_free(name, text);
        if (t == MSG_BYE) break;
    }
    close(sock);
    return NULL;
}
