#define DBG
#include "dbg.h"
#include "properties.h"
#include "main.h"
#include "../shared/message.h"
#include "client_handler.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

chat_node_list_t *g_clients = NULL;
pthread_mutex_t   g_clients_mx = PTHREAD_MUTEX_INITIALIZER;
volatile int      g_shutdown_all = 0;

static volatile int g_stop = 0;
static void on_sigint(int sig){ (void)sig; g_stop = 1; }

static void install_sigint(void) {
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

static int create_listen(uint16_t port) {
    int s = socket(AF_INET, SOCK_STREAM, 0); probe(s>=0, "socket failed");
    int yes=1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_ANY); sa.sin_port = htons(port);
    probe(bind(s, (struct sockaddr*)&sa, sizeof(sa))==0, "bind failed");
    probe(listen(s, 64)==0, "listen failed");
    return s;
error:
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    const char *prop_path = (argc>1 ? argv[1] : "server.properties");
    Properties *props = property_read_properties((char*)prop_path);
    char *p_port = property_get_property(props, "SERVER_PORT");
    uint16_t port = (uint16_t)(p_port ? atoi(p_port) : 7777);

    install_sigint();

    int ls = create_listen(port);
    log_info("[server] listening on port %u", (unsigned)port);

    while (!g_stop && !g_shutdown_all) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cs = accept(ls, (struct sockaddr*)&cli, &cl);
        if (cs < 0) { 
            if (errno==EINTR) 
                continue; 
            log_err("accept failed");
            perror("accept"); 
            break; 
        }
        pthread_t th; pthread_create(&th, NULL, talk_to_client, (void*)(intptr_t)cs); pthread_detach(th);
    }

    // tell everyone BYE then close
    pthread_mutex_lock(&g_clients_mx);
    for (chat_node_list_t *it=g_clients; it; it=it->next) { msg_send(it->node.sock, MSG_BYE, NULL, "Server exiting"); close(it->node.sock); }
    pthread_mutex_unlock(&g_clients_mx);
    close(ls);
    cn_list_free(g_clients);
    return 0;
}
