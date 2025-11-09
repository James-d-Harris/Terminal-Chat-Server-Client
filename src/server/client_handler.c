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

static int send_joining(int s, const char *who)        { return msg_send(s, MSG_JOINING,  who, NULL); }
static int send_left(int s, const char *who)           { return msg_send(s, MSG_LEFT,     who, NULL); }
static int send_deliver(int s, const char *from, const char *txt) { return msg_send(s, MSG_DELIVER, from, txt); }
static int send_bye(int s)                             { return msg_send(s, MSG_BYE, NULL, "Server shutting down"); }

void *talk_to_client(void *arg) {
    int sock = (int)(intptr_t)arg;
    char *cli_name = NULL;
    int joined = 0;

    for (;;) {
        msg_type_t t; char *name=NULL, *text=NULL;
        if (msg_recv(sock, &t, &name, &text) != 0) {
            debug("client socket %d closed or bad frame\n", sock);
            break;
        }

        switch (t) {
        case MSG_JOIN: 
            if (!joined && name && *name) {
                debug("JOIN from %s\n", name);
                int *socks = NULL; size_t sn = 0, cap = 0;
        
                pthread_mutex_lock(&g_clients_mx);
                if (!cn_find_by_name(g_clients, name)) {
                    chat_node_t n = (chat_node_t){0};
                    snprintf(n.name, sizeof(n.name), "%s", name);
                    n.sock = sock;
                    cn_add(&g_clients, &n);
                    joined = 1; cli_name = strdup(name);
        
                    // snapshot recipients (all except joining sock)
                    for (chat_node_list_t *it=g_clients; it; it=it->next) {
                        if (it->node.sock == sock) continue;
                        if (sn == cap) { cap = cap ? cap*2 : 8; socks = realloc(socks, cap*sizeof(int)); }
                        socks[sn++] = it->node.sock;
                    }
                }
                pthread_mutex_unlock(&g_clients_mx);
        
                // send notifications outside the lock
                for (size_t i=0;i<sn;i++) msg_send(socks[i], MSG_JOINING, cli_name, NULL);
                free(socks);
            }
            break;

        case MSG_NOTE: 
            if (joined && text) {
                debug("NOTE from %s: %s\n", cli_name, text);
                int *socks = NULL; size_t sn = 0, cap = 0;
        
                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it=g_clients; it; it=it->next) {
                    if (it->node.sock == sock) continue;
                    if (sn == cap) { cap = cap ? cap*2 : 8; socks = realloc(socks, cap*sizeof(int)); }
                    socks[sn++] = it->node.sock;
                }
                pthread_mutex_unlock(&g_clients_mx);
        
                for (size_t i=0;i<sn;i++) msg_send(socks[i], MSG_DELIVER, cli_name, text);
                free(socks);
            }
            break;
                
        case MSG_LEAVE:
        case MSG_SHUTDOWN: 
            if (joined) {
                debug("LEAVE/SHUTDOWN from %s\n", cli_name ?: "(unknown)");
                int *socks = NULL; size_t sn = 0, cap = 0;
        
                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it=g_clients; it; it=it->next) {
                    if (it->node.sock == sock) continue;
                    if (sn == cap) { cap = cap ? cap*2 : 8; socks = realloc(socks, cap*sizeof(int)); }
                    socks[sn++] = it->node.sock;
                }
                cn_remove_by_sock(&g_clients, sock);
                pthread_mutex_unlock(&g_clients_mx);
        
                for (size_t i=0;i<sn;i++) msg_send(socks[i], MSG_LEFT, cli_name, NULL);
                free(socks);
                joined = 0;
            }
            msg_free(name, text);
            goto done;
        
            

        case MSG_SHUTDOWN_ALL:
            // Entry point: only participants may trigger
            // broadcast BYE to everyone and set server global
            if (joined) {
                g_shutdown_all = 1;
                pthread_mutex_lock(&g_clients_mx);
                for (chat_node_list_t *it=g_clients; it; it=it->next) send_bye(it->node.sock);
                pthread_mutex_unlock(&g_clients_mx);
            }
            msg_free(name, text);
            goto done;

        default:
            // Unknown/ignored
            break;
        }

        msg_free(name, text);
    }

done:
    if (cli_name) free(cli_name);
    close(sock);
    return NULL;
}
