#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

/*
 * send_all
 * --------
 * Sends exactly `len` bytes on the socket (even if send() only sends partial).
 * Returns:
 *   0 on success
 *  -1 on failure or socket closure.
 */
int send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = buf;
    while (len > 0) {
        ssize_t sent = send(fd, p, len, 0);
        if (sent <= 0) return -1;
        p   += sent;
        len -= (size_t)sent;
    }
    return 0;
}

/*
 * recv_all
 * --------
 * Receives exactly `len` bytes.
 * Returns:
 *   0 on success
 *  -1 if socket closed or recv fails.
 */
int recv_all(int fd, void *buf, size_t len) {
    unsigned char *p = buf;
    while (len > 0) {
        ssize_t recvd = recv(fd, p, len, 0);
        if (recvd <= 0) return -1;
        p   += recvd;
        len -= (size_t)recvd;
    }
    return 0;
}

/*
 * msg_send
 * --------
 * Framed send: [uint32 wire_len][header][name][text]
 *
 * wire_len = sizeof(header) + name_len + text_len (in network byte order).
 *
 * This keeps protocol decoding correct and future-proof.
 */
int msg_send(int sock, msg_type_t type, const char *name, const char *text) {
    uint32_t name_len = name ? (uint32_t)strlen(name) : 0;
    uint32_t text_len = text ? (uint32_t)strlen(text) : 0;

    msg_hdr_t header = {
        htonl((uint32_t)type),
        htonl(name_len),
        htonl(text_len)
    };

    uint32_t total_body_len = sizeof(header) + name_len + text_len;
    uint32_t wire_len = htonl(total_body_len);

    if (send_all(sock, &wire_len, sizeof(wire_len))) return -1;
    if (send_all(sock, &header, sizeof(header))) return -1;
    if (name_len && send_all(sock, name, name_len)) return -1;
    if (text_len && send_all(sock, text, text_len)) return -1;
    return 0;
}

/*
 * msg_recv
 * --------
 * Parses a framed message as sent by msg_send().
 * Allocates buffers for name/text if present; caller must free via msg_free().
 * Returns:
 *   0 on success
 *  -1 on malformed frame or socket error.
 */
int msg_recv(int sock, msg_type_t *type, char **name_out, char **text_out) {
    uint32_t wire_len_net;
    if (recv_all(sock, &wire_len_net, sizeof(wire_len_net))) return -1;

    uint32_t body_len = ntohl(wire_len_net);

    /* Basic sanity check (32MB max cap) */
    if (body_len < sizeof(msg_hdr_t) || body_len > (32u << 20))
        return -1;

    msg_hdr_t header;
    if (recv_all(sock, &header, sizeof(header))) return -1;

    uint32_t type_val = ntohl(header.type);
    uint32_t name_len = ntohl(header.name_len);
    uint32_t text_len = ntohl(header.text_len);

    /* Validate that lengths add up exactly */
    if (sizeof(header) + name_len + text_len != body_len)
        return -1;

    char *name_buf = NULL;
    char *text_buf = NULL;

    if (name_len) {
        name_buf = malloc(name_len + 1);
        if (!name_buf) return -1;
        if (recv_all(sock, name_buf, name_len)) { free(name_buf); return -1; }
        name_buf[name_len] = '\0';
    }

    if (text_len) {
        text_buf = malloc(text_len + 1);
        if (!text_buf) { free(name_buf); return -1; }
        if (recv_all(sock, text_buf, text_len)) { free(name_buf); free(text_buf); return -1; }
        text_buf[text_len] = '\0';
    }

    *type = (msg_type_t)type_val;
    if (name_out) *name_out = name_buf; else free(name_buf);
    if (text_out) *text_out = text_buf; else free(text_buf);

    return 0;
}

/*
 * msg_free
 * --------
 * Frees the name and text buffers allocated by msg_recv().
 */
void msg_free(char *name, char *text) {
    free(name);
    free(text);
}
