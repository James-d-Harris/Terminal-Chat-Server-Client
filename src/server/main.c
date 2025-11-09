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

/*
    GLOBAL SERVER STATE
    -------------------
    g_clients        : Linked list holding all currently connected chat participants.
    g_clients_mx     : Mutex protecting concurrent access to g_clients.
    g_shutdown_all   : Set to 1 when a client requests "SHUTDOWN ALL". Causes server to exit.
    g_stop           : Local stop flag set when Ctrl-C is pressed. Causes server to exit.
*/
chat_node_list_t *g_clients = NULL;
pthread_mutex_t   g_clients_mx = PTHREAD_MUTEX_INITIALIZER;
volatile int      g_shutdown_all = 0;
static volatile int g_stop = 0;

/*
    Ctrl-C Signal Handler
    ---------------------
    We do not exit immediately inside the handler (not async-safe).
    Instead, we set g_stop and allow the main loop to exit cleanly.
*/
static void on_sigint(int unused_signal) {
    (void)unused_signal;
    g_stop = 1;
}

/*
    Install Ctrl-C Handler (no SA_RESTART)
    --------------------------------------
    Ensures accept() will break when Ctrl-C is pressed on macOS/Linux.
*/
static void install_sigint_handler(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = on_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0; // NO SA_RESTART → allows accept() to be interrupted

    sigaction(SIGINT, &action, NULL);
}

/*
    create_listen(port)
    -------------------
    Creates a TCP socket, binds it to the requested port, and puts it into listening mode.
*/
static int create_listening_socket(uint16_t listening_port) {
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    probe(listen_socket >= 0, "socket creation failed");

    // Allow fast restart if server was just stopped
    int enable_reuse = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all local interfaces
    server_addr.sin_port = htons(listening_port);

    probe(bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0, "bind failed");
    probe(listen(listen_socket, 64) == 0, "listen failed");

    return listen_socket;

error:
    exit(EXIT_FAILURE);
}

/*
    main()
    ------
    Reads server port from properties file (default: server.properties).
    Creates listening socket.
    Loop:
        - Accept new clients.
        - Spawn a thread to handle each client independently.
    When shutting down:
        - Notify connected clients with MSG_BYE.
        - Close all sockets.
*/
int main(int argc, char **argv) {
    // Determine properties file to load
    const char *properties_path = (argc > 1 ? argv[1] : "server.properties");

    // Read configuration
    Properties *server_properties = property_read_properties((char*)properties_path);
    char *port_string = property_get_property(server_properties, "SERVER_PORT");
    uint16_t listening_port = (uint16_t)(port_string ? atoi(port_string) : 7777);

    // Enable Ctrl-C exit
    install_sigint_handler();

    // Create server listening socket
    int listening_socket = create_listening_socket(listening_port);
    log_info("[server] listening on port %u", (unsigned)listening_port);

    /*
        ACCEPT LOOP
        -----------
        Continues until:
        - Ctrl-C occurs   → g_stop = 1
        - Client triggers SHUTDOWN ALL → g_shutdown_all = 1
    */
    while (!g_stop && !g_shutdown_all) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(listening_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_socket < 0) {
            // EINTR means accept() was interrupted by signal → loop exits naturally
            if (errno == EINTR)
                continue;

            log_err("accept failed");
            perror("accept");
            break;
        }

        // Spawn a dedicated handler per client
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, talk_to_client, (void*)(intptr_t)client_socket);
        pthread_detach(client_thread);
    }

    /*
        SERVER SHUTDOWN SEQUENCE
        ------------------------
        Notify all remaining clients that the server is going away.
        Close all sockets.
    */
    pthread_mutex_lock(&g_clients_mx);
    for (chat_node_list_t *node = g_clients; node; node = node->next) {
        msg_send(node->node.sock, MSG_BYE, NULL, "Server exiting");
        close(node->node.sock);
    }
    pthread_mutex_unlock(&g_clients_mx);

    close(listening_socket);
    cn_list_free(g_clients);

    return 0;
}
