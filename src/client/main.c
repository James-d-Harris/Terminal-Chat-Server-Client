#define DBG
#include "dbg.h"
#include "properties.h"
#include "main.h"
#include "receiver_handler.h"
#include "sender_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *prop_path = (argc>1 ? argv[1] : "client.properties");
    Properties *props = property_read_properties((char*)prop_path);

    client_cfg_t cfg = {0};
    char *n  = property_get_property(props, "CLIENT_NAME");
    char *ip = property_get_property(props, "SERVER_IP");
    char *pp = property_get_property(props, "SERVER_PORT");

    snprintf(cfg.name, sizeof(cfg.name),  "%s", n  ? n  : "Anonymous");
    snprintf(cfg.server_ip, sizeof(cfg.server_ip), "%s", ip ? ip : "127.0.0.1");
    cfg.server_port = (uint16_t)(pp ? atoi(pp) : 7777);

    sender_ctx_t sctx = { .sock = -1, .quit = 0 };
    snprintf(sctx.my_name, sizeof(sctx.my_name), "%s", cfg.name);
    snprintf(sctx.server_ip, sizeof(sctx.server_ip), "%s", cfg.server_ip);
    sctx.server_port = cfg.server_port;

    printf("Commands:\n  JOIN IP port\n  LEAVE\n  SHUTDOWN\n  SHUTDOWN ALL\n  <any text> -> NOTE\n");

    pthread_t th_sender; pthread_create(&th_sender, NULL, sender_thread, &sctx);

    pthread_t th_receiver = 0;
    int receiver_running = 0;
    while (!sctx.quit) {
        if (!receiver_running && sctx.sock >= 0) {
            receiver_running = 1;
            pthread_create(&th_receiver, NULL, receiver_thread, (void*)(intptr_t)sctx.sock);
        }
        usleep(50*1000);
    }

    if (receiver_running) pthread_join(th_receiver, NULL);
    pthread_join(th_sender, NULL);
    return 0;
}
