#ifndef SEV_H
#define SEV_H

#include <stdlib.h>
#include <sys/queue.h>
#include <ev.h>

struct sev_client;

typedef void (sev_open_cb)(struct sev_client *client);
typedef void (sev_read_cb)(struct sev_client *client, const char *data,
    size_t len);
typedef void (sev_close_cb)(struct sev_client *client);

struct sev_server {
    // socket descriptor
    int sd;

    // libev watcher
    struct ev_io *watcher;

    // callbacks
    sev_open_cb *open_cb;
    sev_read_cb *read_cb;
    sev_close_cb *close_cb;

    // user data
    void *data;
};

struct sev_buffer {
    size_t len;
    size_t start;
    char *data;

    STAILQ_ENTRY(sev_buffer) entries;
};

struct sev_client {
    // socket descriptor
    int sd;

    // libev watchers
    struct ev_io *w_read;
    struct ev_io *w_write;
    int writing;

    // client info
    char *ip;
    int port;

    struct sev_server *server;

    // user data
    void *data;

    // write queue
    STAILQ_HEAD(sev_buffer_head, sev_buffer) head;
};

int sev_server_init(struct sev_server *server, int port);

void sev_send(struct sev_client *client, const char *data, size_t len);

void sev_close(struct sev_client *client);

#endif
