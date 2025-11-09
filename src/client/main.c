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

/*
 * Client entry:
 * - Read config from client.properties (CLIENT_NAME, SERVER_IP, SERVER_PORT).
 * - Initialize sender context with that configuration.
 * - Start the sender thread (reads stdin, issues JOIN/LEAVE/NOTE/SHUTDOWN).
 * - Lazily start the receiver thread after JOIN succeeds (when sock >= 0).
 * - Wait for threads to finish and exit.
 */
int main(int argc, char **argv) {
    const char *properties_path = (argc>1 ? argv[1] : "client.properties");
    Properties *client_properties = property_read_properties((char*)properties_path);

    client_cfg_t loaded_cfg = (client_cfg_t){0};
    char *prop_client_name  = property_get_property(client_properties, "CLIENT_NAME");
    char *prop_server_ip    = property_get_property(client_properties, "SERVER_IP");
    char *prop_server_port  = property_get_property(client_properties, "SERVER_PORT");

    snprintf(loaded_cfg.name, sizeof(loaded_cfg.name), "%s", prop_client_name ? prop_client_name : "Anonymous");
    snprintf(loaded_cfg.server_ip, sizeof(loaded_cfg.server_ip), "%s", prop_server_ip ? prop_server_ip : "127.0.0.1");
    loaded_cfg.server_port = (uint16_t)(prop_server_port ? atoi(prop_server_port) : 7777);

    sender_ctx_t sender_ctx = { .sock = -1, .quit = 0 };
    snprintf(sender_ctx.my_name, sizeof(sender_ctx.my_name), "%s", loaded_cfg.name);
    snprintf(sender_ctx.server_ip, sizeof(sender_ctx.server_ip), "%s", loaded_cfg.server_ip);
    sender_ctx.server_port = loaded_cfg.server_port;

    printf("Commands:\n  JOIN IP port\n  LEAVE\n  SHUTDOWN\n  SHUTDOWN ALL\n  <any text> -> NOTE\n");

    /* Sender thread: parses user commands from stdin and talks to the server. */
    pthread_t sender_thread_id;
    pthread_create(&sender_thread_id, NULL, sender_thread, &sender_ctx);

    /*
     * Receiver thread lifecycle:
     * - We don't create it until the first successful JOIN (when sender sets sock >= 0).
     * - This avoids having a receiver spinning on an invalid socket before JOIN.
     */
    pthread_t receiver_thread_id = 0;
    int receiver_running = 0;
    while (!sender_ctx.quit) {
        if (!receiver_running && sender_ctx.sock >= 0) {
            receiver_running = 1;
            pthread_create(&receiver_thread_id, NULL, receiver_thread, (void*)(intptr_t)sender_ctx.sock);
        }
        /* Poll at a small interval to avoid busy-waiting while keeping code simple. */
        usleep(50*1000);
    }

    if (receiver_running) pthread_join(receiver_thread_id, NULL);
    pthread_join(sender_thread_id, NULL);
    return 0;
}
