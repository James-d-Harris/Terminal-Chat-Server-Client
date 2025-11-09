#pragma once
#include <stdint.h>
#include <stddef.h>

// Message types
typedef enum {
    MSG_JOIN = 1,
    MSG_LEAVE = 2,
    MSG_NOTE = 3,
    MSG_SHUTDOWN = 4,
    MSG_SHUTDOWN_ALL = 5,

    // server -> client indications
    MSG_JOINING = 10,
    MSG_LEFT = 11,
    MSG_DELIVER = 12,
    MSG_BYE = 13
} msg_type_t;

typedef struct {
    uint32_t type;
    uint32_t name_len;
    uint32_t text_len;
} msg_hdr_t;

int  msg_send(int sock, msg_type_t type, const char *name, const char *text);
int  msg_recv(int sock, msg_type_t *type, char **name_out, char **text_out);
void msg_free(char *name, char *text);

int  send_all(int fd, const void *buf, size_t len);
int  recv_all(int fd, void *buf, size_t len);
