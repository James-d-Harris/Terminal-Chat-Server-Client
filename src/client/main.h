#pragma once
#include <stdint.h>

typedef struct {
    char   name[64];
    char   server_ip[64];
    uint16_t server_port;
} client_cfg_t;

int connect_to_server(const char *ip, uint16_t port);
