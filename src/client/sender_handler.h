#pragma once
#include <stdatomic.h>

typedef struct {
    int sock;                      // -1 if not joined
    char my_name[64];
    char server_ip[64];
    unsigned short server_port;
    _Atomic int quit;              // set when SHUTDOWN or server BYE
} sender_ctx_t;

void *sender_thread(void *arg); // arg = (sender_ctx_t*)
