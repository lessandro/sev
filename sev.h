#ifndef SEV_H
#define SEV_H

#include <stdlib.h>

struct sev_client;

typedef void (sev_open_cb)(struct sev_client *client);
typedef void (sev_read_cb)(struct sev_client *client, const char *data,
    size_t len);
typedef void (sev_close_cb)(struct sev_client *client);

struct sev_server {
    int sd;
    void *watcher;

    sev_open_cb *open_cb;
    sev_read_cb *read_cb;
    sev_close_cb *close_cb;

    void *data;
};

struct sev_client {
    int sd;
    void *watcher;

    char *ip;
    int port;

    struct sev_server *server;

    void *data;
};

int sev_server_init(struct sev_server *server, int port);

void sev_send(struct sev_client *client, const char *data, size_t len);

void sev_close(struct sev_client *client);

#endif
