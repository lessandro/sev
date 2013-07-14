#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ev.h>
#include "sev.h"

#define BUFSIZE 2048 // fits a 1500-byte MTU packet

static void client_write(struct sev_client *client)
{
    // write
}

static void client_close(struct sev_client *client)
{
    if (client->server->close_cb)
        client->server->close_cb(client);

    if (close(client->sd) == -1) {
        perror("close");
    }

    ev_io_stop(EV_DEFAULT_ client->watcher);
    free(client->watcher);
    free(client->ip);
    free(client);
}

static void client_read(struct sev_client *client)
{
    static char buffer[BUFSIZE];
    ssize_t n = recv(client->sd, buffer, BUFSIZE - 1, 0);

    if (n < 0) {
        // error
        perror("recv");
        return;
    }

    if (n == 0) {
        // client disconnected
        client_close(client);
        return;
    }

    if (client->server->read_cb)
        client->server->read_cb(client, buffer, n);
}

static void client_cb(EV_P_ struct ev_io *watcher, int revents)
{
    if (revents & EV_ERROR) {
        perror("client_cb");
        return;
    }

    if (revents & EV_WRITE)
        client_write(watcher->data);

    if (revents & EV_READ)
        client_read(watcher->data);
}

static void accept_cb(EV_P_ struct ev_io *watcher, int revents)
{
    // accept client socket
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int sd = accept(watcher->fd, (struct sockaddr*)&addr, &addr_len);
    if (sd == -1) {
        perror("accept");
        return;
    }

    // set non-blocking
    int flags = fcntl(sd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(sd, F_SETFL, flags);

    // disable nagle's algorithm
    int flag = 1;
    setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    // register with libev
    struct ev_io *w_client = malloc(sizeof(struct ev_io));
    ev_io_init(w_client, client_cb, sd, EV_READ);
    ev_io_start(EV_DEFAULT_ w_client);

    // initialize sev_client structure
    struct sev_server *server = watcher->data;
    struct sev_client *client = malloc(sizeof(struct sev_client));

    client->sd = sd;
    client->watcher = w_client;
    client->server = server;
    client->port = addr.sin_port;
    client->ip = malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &addr.sin_addr, client->ip, INET_ADDRSTRLEN);

    w_client->data = client;

    // call open callback
    if (server->open_cb)
        server->open_cb(client);
}

int sev_server_init(struct sev_server *server, int port)
{
    // create server socket
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if (sd == -1)
        return -1;

    // set reuseaddr
    int flag = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // bind/listen
    if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        return -1;
    if (listen(sd, SOMAXCONN) == -1)
        return -1;

    // register with libev
    struct ev_io *watcher = malloc(sizeof(struct ev_io));
    ev_io_init(watcher, accept_cb, sd, EV_READ);
    watcher->data = server;
    ev_io_start(EV_DEFAULT_ watcher);

    // initialize sev_server structure
    memset(server, 0, sizeof(struct sev_server));
    server->sd = sd;
    server->watcher = watcher;

    return 0;
}

void sev_close(struct sev_client *client)
{
    client_close(client);
}

void sev_send(struct sev_client *client, const char *data, size_t len)
{
    printf("todo: sev_send\n");
}
