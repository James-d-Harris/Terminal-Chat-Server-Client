#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = buf;
    while (len) { ssize_t n = send(fd, p, len, 0); if (n <= 0) return -1; p += n; len -= (size_t)n; }
    return 0;
}
int recv_all(int fd, void *buf, size_t len) {
    unsigned char *p = buf;
    while (len) { ssize_t n = recv(fd, p, len, 0); if (n <= 0) return -1; p += n; len -= (size_t)n; }
    return 0;
}

int msg_send(int sock, msg_type_t type, const char *name, const char *text) {
    uint32_t nlen = name ? (uint32_t)strlen(name) : 0;
    uint32_t tlen = text ? (uint32_t)strlen(text) : 0;

    msg_hdr_t hdr = { htonl((uint32_t)type), htonl(nlen), htonl(tlen) };
    uint32_t body_len = sizeof(hdr) + nlen + tlen;
    uint32_t wire_len = htonl(body_len);

    if (send_all(sock, &wire_len, sizeof(wire_len))) return -1;
    if (send_all(sock, &hdr, sizeof(hdr))) return -1;
    if (nlen && send_all(sock, name, nlen)) return -1;
    if (tlen && send_all(sock, text, tlen)) return -1;
    return 0;
}

int msg_recv(int sock, msg_type_t *type, char **name_out, char **text_out) {
    uint32_t len_net;
    if (recv_all(sock, &len_net, sizeof(len_net))) return -1;
    uint32_t body_len = ntohl(len_net);
    if (body_len < sizeof(msg_hdr_t) || body_len > (32u<<20)) return -1;

    msg_hdr_t hdr;
    if (recv_all(sock, &hdr, sizeof(hdr))) return -1;
    uint32_t t = ntohl(hdr.type), nln = ntohl(hdr.name_len), tln = ntohl(hdr.text_len);
    if (sizeof(hdr) + nln + tln != body_len) return -1;

    char *name = NULL, *text = NULL;
    if (nln) {
        name = malloc(nln+1); if(!name) return -1;
        if (recv_all(sock, name, nln)) { free(name); return -1; }
        name[nln] = '\0';
    }
    if (tln) {
        text = malloc(tln+1); if(!text) { free(name); return -1; }
        if (recv_all(sock, text, tln)) { free(name); free(text); return -1; }
        text[tln] = '\0';
    }

    *type = (msg_type_t)t;
    if (name_out) *name_out = name; else free(name);
    if (text_out) *text_out = text; else free(text);
    return 0;
}

void msg_free(char *name, char *text) { free(name); free(text); }
