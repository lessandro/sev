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

// sev_stream

static void sev_stream_free(struct sev_stream *stream)
{
    // free write queue
    sev_queue_free(stream->queue);

    // free everything
    free(stream->w_read);
    free(stream->w_write);
    free(stream->remote_address);
    free(stream);
}

// callbacks

static void stream_write(struct sev_stream *stream)
{
    struct sev_buffer *buffer = sev_queue_head(stream->queue);

    char *data = buffer->data + buffer->start;
    ssize_t len = buffer->len - buffer->start;

    int n = send(stream->sd, data, len, 0);

    if (n == -1) {
        perror("send");
        return;
    }

    buffer->start += n;

    if (buffer->start == buffer->len) {
        sev_queue_free_head(stream->queue);

        if (sev_queue_head(stream->queue) == NULL) {
            // nothing left to write
            stream->writing = 0;
            ev_io_stop(EV_DEFAULT_ stream->w_write);
        }
    }
}

static void stream_close(struct sev_stream *stream)
{
    if (stream->server->close_cb)
        stream->server->close_cb(stream);

    if (close(stream->sd) == -1) {
        perror("close");
    }

    // stop libev watchers
    ev_io_stop(EV_DEFAULT_ stream->w_read);
    if (stream->writing)
        ev_io_stop(EV_DEFAULT_ stream->w_write);

    sev_stream_free(stream);
}

static void stream_read(struct sev_stream *stream)
{
    static char buffer[BUFSIZE];
    ssize_t n = recv(stream->sd, buffer, BUFSIZE - 1, 0);

    if (n < 0) {
        // error
        perror("recv");
        return;
    }

    if (n == 0) {
        // client disconnected
        stream_close(stream);
        return;
    }

    if (stream->server->read_cb)
        stream->server->read_cb(stream, buffer, n);
}

static void stream_cb(EV_P_ struct ev_io *watcher, int revents)
{
    if (revents & EV_ERROR) {
        perror("stream_cb");
        return;
    }

    if (revents & EV_WRITE)
        stream_write(watcher->data);

    if (revents & EV_READ)
        stream_read(watcher->data);
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
    stream->remote_address = malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &addr.sin_addr, stream->remote_address,
        INET_ADDRSTRLEN);

    // register with libev
    stream->w_read = malloc(sizeof(struct ev_io));
    ev_io_init(stream->w_read, stream_cb, sd, EV_READ);
    ev_io_start(EV_DEFAULT_ stream->w_read);

    stream->w_write = malloc(sizeof(struct ev_io));
    ev_io_init(stream->w_write, stream_cb, sd, EV_WRITE);

    stream->w_read->data = stream;
    stream->w_write->data = stream;
    stream->writing = 0;

    // initialize write queue
    stream->queue = sev_queue_new();

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

void sev_close(struct sev_stream *stream)
{
    stream_close(stream);
}

void sev_send(struct sev_stream *stream, const char *data, size_t len)
{
    sev_queue_push_back(stream->queue, data, len);

    if (!stream->writing) {
        ev_io_start(EV_DEFAULT_ stream->w_write);
        stream->writing = 1;
    }
}
