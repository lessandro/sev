/*-
 * Copyright (c) 2013, Lessandro Mariano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <ev.h>
#include "sev.h"

#define RECV_BUFFER_SIZE 2048 // fits a 1500-byte MTU packet

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// callbacks

static void stream_cb(EV_P_ struct ev_io *watcher, int revents)
{
    if (revents & EV_ERROR) {
        perror("stream_cb");
        sev_close(watcher->data, strerror(errno));
        return;
    }

    if ((revents & EV_READ) == 0)
        return;

    struct sev_stream *stream = watcher->data;

    static char buffer[RECV_BUFFER_SIZE];
    ssize_t n = recv(stream->sd, buffer, RECV_BUFFER_SIZE - 1, 0);

    if (n < 0) {
        // error
        perror("recv");
        sev_close(stream, strerror(errno));
        return;
    }

    if (n == 0) {
        // client disconnected
        sev_close(stream, strerror(ECONNRESET));
        return;
    }

    if (stream->server->read_cb)
        stream->server->read_cb(stream, buffer, n);
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

    // initialize sev_stream structure
    struct sev_server *server = watcher->data;
    struct sev_stream *stream = malloc(sizeof(struct sev_stream));

    stream->sd = sd;
    stream->server = server;
    stream->remote_port = addr.sin_port;
    inet_ntop(AF_INET, &addr.sin_addr, stream->remote_address,
        INET_ADDRSTRLEN);

    // register with libev
    ev_io_init(&stream->watcher, stream_cb, sd, EV_READ);
    ev_io_start(EV_DEFAULT_ &stream->watcher);

    stream->watcher.data = stream;

    // call open callback
    if (server->open_cb)
        server->open_cb(stream);
}

// interface

int sev_listen(struct sev_server *server, int port)
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

    // initialize sev_server structure
    memset(server, 0, sizeof(struct sev_server));
    server->sd = sd;

    // register with libev
    ev_io_init(&server->watcher, accept_cb, sd, EV_READ);
    server->watcher.data = server;
    ev_io_start(EV_DEFAULT_ &server->watcher);

    return 0;
}

void sev_close(struct sev_stream *stream, const char *reason)
{
    if (stream->server->close_cb)
        stream->server->close_cb(stream, reason);

    if (close(stream->sd) == -1)
        perror("close");

    // stop libev watcher
    ev_io_stop(EV_DEFAULT_ &stream->watcher);

    memset(stream, -1, sizeof(struct sev_stream));
    free(stream);
}

int sev_send(struct sev_stream *stream, const char *data, size_t len)
{
    while (len > 0) {
        int n = send(stream->sd, data, len, 0);

        if (n == -1) {
            perror("sev_send");
            sev_close(stream, strerror(errno));
            return -1;
        }

        // can send() return 0?
        data += n;
        len -= n;
    }

    return 0;
}
