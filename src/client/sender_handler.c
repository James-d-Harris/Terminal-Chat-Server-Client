#define DBG
#include "dbg.h"
#include "sender_handler.h"
#include "../shared/message.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

static int do_join(sender_ctx_t *ctx) {
    if (ctx->sock >= 0) { printf("[warn] already joined\n"); return 0; }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET; sa.sin_port = htons(ctx->server_port);
    if (inet_pton(AF_INET, ctx->server_ip, &sa.sin_addr) != 1) { log_err("bad IP"); close(s); return -1; }
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) < 0) { log_err("connect failed"); close(s); return -1; }
    if (msg_send(s, MSG_JOIN, ctx->my_name, NULL)!=0) { log_err("JOIN send failed"); close(s); return -1; }
    ctx->sock = s;
    printf("[info] joined %s:%u as %s\n", ctx->server_ip, ctx->server_port, ctx->my_name);
    return 0;
}

static int do_leave(sender_ctx_t *ctx) {
    if (ctx->sock < 0) { printf("[warn] not joined\n"); return 0; }
    msg_send(ctx->sock, MSG_LEAVE, NULL, NULL);
    close(ctx->sock); ctx->sock = -1;
    printf("[info] left chat\n");
    return 0;
}

void *sender_thread(void *arg) {
    sender_ctx_t *ctx = (sender_ctx_t*)arg;
    char line[2048];

    while (!ctx->quit) {
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;

        if (!strncmp(line, "JOIN ", 5)) {
            char ip[64]; int port=0;
            if (sscanf(line+5, "%63s %d", ip, &port)==2) {
                strncpy(ctx->server_ip, ip, sizeof(ctx->server_ip)-1);
                ctx->server_port = (unsigned short)port;
            }
            do_join(ctx);
        } else if (!strcmp(line, "LEAVE")) {
            do_leave(ctx);
        } else if (!strcmp(line, "SHUTDOWN ALL")) {
            if (ctx->sock >= 0) msg_send(ctx->sock, MSG_SHUTDOWN_ALL, NULL, NULL);
            ctx->quit = 1;
        } else if (!strcmp(line, "SHUTDOWN")) {
            if (ctx->sock >= 0) msg_send(ctx->sock, MSG_SHUTDOWN, NULL, NULL);
            if (ctx->sock >= 0) { close(ctx->sock); ctx->sock = -1; }
            ctx->quit = 1;
        } else {
            if (ctx->sock < 0) {
                printf("[warn] you must JOIN before sending notes\n");
            } else {
                msg_send(ctx->sock, MSG_NOTE, NULL, line);
            }
        }
    }
    if (ctx->sock >= 0) close(ctx->sock);
    return NULL;
}
