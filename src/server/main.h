#pragma once
#include <pthread.h>
#include "../shared/chat_node.h"

extern chat_node_list_t *g_clients;
extern pthread_mutex_t   g_clients_mx;
extern volatile int      g_shutdown_all;
