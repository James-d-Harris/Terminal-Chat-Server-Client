#define DBG
#include "dbg.h"
#include "sender_handler.h"
#include "../shared/message.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

/*
 * Connect to the server and send a JOIN with our configured name.
 * On success, sets ctx->sock to the connected socket and prints a short status line.
 */
static int do_join(sender_ctx_t *ctx) {
    if (ctx->sock >= 0) { printf("[warn] already joined\n"); return 0; }

    int new_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->server_port);

    if (inet_pton(AF_INET, ctx->server_ip, &server_addr.sin_addr) != 1) {
        log_err("bad IP");
        close(new_socket_fd);
        return -1;
    }
    if (connect(new_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_err("connect failed");
        close(new_socket_fd);
        return -1;
    }
    if (msg_send(new_socket_fd, MSG_JOIN, ctx->my_name, NULL) != 0) {
        log_err("JOIN send failed");
        close(new_socket_fd);
        return -1;
    }

    ctx->sock = new_socket_fd;
    printf("[info] joined %s:%u as %s\n", ctx->server_ip, ctx->server_port, ctx->my_name);
    return 0;
}

/*
 * Send LEAVE and close our current socket.
 * After this, the user may JOIN again to the same or a different server.
 */
static int do_leave(sender_ctx_t *ctx) {
    if (ctx->sock < 0) { printf("[warn] not joined\n"); return 0; }
    msg_send(ctx->sock, MSG_LEAVE, NULL, NULL);
    close(ctx->sock);
    ctx->sock = -1;
    printf("[info] left chat\n");
    return 0;
}

/*
 * Sender thread:
 * - Reads lines from stdin (blocking).
 * - Recognized commands:
 *     "JOIN IP port"   → connects and sends JOIN (updates ctx->server_ip/port if provided)
 *     "LEAVE"          → sends LEAVE and closes the socket
 *     "SHUTDOWN"       → sends SHUTDOWN (leaves if joined), then sets quit flag
 *     "SHUTDOWN ALL"   → sends SHUTDOWN_ALL (only valid if joined), then sets quit flag
 *   Any other text     → sent as NOTE to all other clients (must be joined)
 */
void *sender_thread(void *arg) {
    sender_ctx_t *ctx = (sender_ctx_t*)arg;
    char input_line[2048];

    while (!ctx->quit) {
        if (!fgets(input_line, sizeof(input_line), stdin)) break;
        input_line[strcspn(input_line, "\r\n")] = 0; /* strip newline */
        if (!*input_line) continue;

        if (!strncmp(input_line, "JOIN ", 5)) {
            char ip_str[64]; int port_val = 0;
            if (sscanf(input_line+5, "%63s %d", ip_str, &port_val) == 2) {
                /* Update destination from user command before attempting the join. */
                strncpy(ctx->server_ip, ip_str, sizeof(ctx->server_ip)-1);
                ctx->server_port = (unsigned short)port_val;
            }
            do_join(ctx);

        } else if (!strcmp(input_line, "LEAVE")) {
            do_leave(ctx);

        } else if (!strcmp(input_line, "SHUTDOWN ALL")) {
            if (ctx->sock >= 0) msg_send(ctx->sock, MSG_SHUTDOWN_ALL, NULL, NULL);
            ctx->quit = 1;

        } else if (!strcmp(input_line, "SHUTDOWN")) {
            if (ctx->sock >= 0) msg_send(ctx->sock, MSG_SHUTDOWN, NULL, NULL);
            if (ctx->sock >= 0) { close(ctx->sock); ctx->sock = -1; }
            ctx->quit = 1;

        } else {
            /* Default: treat as NOTE, but only if we are currently a chat participant. */
            if (ctx->sock < 0) {
                printf("[warn] you must JOIN before sending notes\n");
            } else {
                msg_send(ctx->sock, MSG_NOTE, NULL, input_line);
            }
        }
    }

    if (ctx->sock >= 0) close(ctx->sock);
    return NULL;
}
