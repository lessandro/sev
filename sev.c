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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <ev.h>
#include "sev.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// callbacks

static void stream_write(struct sev_stream *stream)
{
    int len = MIN(stream->buffer_len, SEND_BUFFER_SIZE - stream->buffer_start);

    int n = send(stream->sd, stream->buffer + stream->buffer_start, len, 0);

    if (n == -1) {
        sev_close(stream, strerror(errno));
        return;
    }

    stream->buffer_start = (stream->buffer_start + n) % SEND_BUFFER_SIZE;
    stream->buffer_len -= n;

    if (stream->buffer_len == 0) {
        // reset circular buffer to the beginning
        stream->buffer_start = 0;
        stream->buffer_end = 0;

        stream->writing = 0;
        ev_io_stop(EV_DEFAULT_ &stream->w_write);
    }
}

static void stream_read(struct sev_stream *stream)
{
    static char buffer[RECV_BUFFER_SIZE];
    ssize_t n = recv(stream->sd, buffer, RECV_BUFFER_SIZE - 1, 0);

    if (n == -1) {
        // error
        sev_close(stream, strerror(errno));
        return;
    }

    if (n == 0) {
        // client disconnected
        sev_close(stream, strerror(ECONNRESET));
        return;
    }

    if (stream->read_cb)
        stream->read_cb(stream, buffer, n);
}

static void stream_cb(EV_P_ struct ev_io *watcher, int revents)
{
    if (revents & EV_ERROR) {
        sev_close(watcher->data, strerror(errno));
        return;
    }

    if (revents & EV_READ) {
        stream_read(watcher->data);
        return;
    }

    if (revents & EV_WRITE) {
        stream_write(watcher->data);
        return;
    }
}

static struct sev_stream *sev_stream_new(int sd, struct sockaddr_in *addr)
{
    // set non-blocking
    int flags = fcntl(sd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(sd, F_SETFL, flags);

    // disable nagle's algorithm
    int flag = 1;
    setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    // initialize sev_stream structure
    struct sev_stream *stream = calloc(1, sizeof(struct sev_stream));

    stream->sd = sd;
    stream->remote_port = addr->sin_port;
    inet_ntop(AF_INET, &addr->sin_addr, stream->remote_address,
        INET_ADDRSTRLEN);

    // register with libev
    ev_io_init(&stream->w_read, stream_cb, sd, EV_READ);
    ev_io_start(EV_DEFAULT_ &stream->w_read);
    stream->reading = 1;

    ev_io_init(&stream->w_write, stream_cb, sd, EV_WRITE);
    stream->writing = 0;

    stream->w_read.data = stream;
    stream->w_write.data = stream;

    return stream;
}

static void accept_cb(EV_P_ struct ev_io *watcher, int revents)
{
    // accept client socket
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int sd = accept(watcher->fd, (struct sockaddr *)&addr, &addr_len);
    if (sd == -1)
        return;

    struct sev_stream *stream = sev_stream_new(sd, &addr);

    struct sev_server *server = watcher->data;
    stream->server = server;

    // initialize callbacks
    stream->read_cb = server->read_cb;
    stream->close_cb = server->close_cb;

    // call open callback
    if (server->open_cb)
        server->open_cb(stream);
}

// interface

int sev_send(struct sev_stream *stream, const char *data, size_t len)
{
    // try sending the data straight away
    if (!stream->writing) {
        int n = send(stream->sd, data, len, 0);

        if (n == -1) {
            if (errno != EAGAIN) {
                sev_close(stream, strerror(errno));
                return -1;
            }
        }
        else if (n < len) {
            // sent part of the data
            data += n;
            len -= n;
        }
        else {
            // sent all the data, nothing else to do here
            return 0;
        }
    }

    // buffer full
    if (stream->buffer_len + len > SEND_BUFFER_SIZE) {
        sev_close(stream, "Send buffer full");
        return -1;        
    }

    // write the data to the circular buffer
    size_t first_part = MIN(SEND_BUFFER_SIZE - stream->buffer_end, len);
    size_t second_part = len - first_part;

    memcpy(stream->buffer + stream->buffer_end, data, first_part);
    memcpy(stream->buffer, data + first_part, second_part);

    stream->buffer_end = (stream->buffer_end + len) % SEND_BUFFER_SIZE;
    stream->buffer_len += len;

    // tell libev we want to write
    if (!stream->writing) {
        ev_io_start(EV_DEFAULT_ &stream->w_write);
        stream->writing = 1;
    }

    return 0;
}

void sev_close(struct sev_stream *stream, const char *reason)
{
    if (stream->close_cb)
        stream->close_cb(stream, reason);

    // stop libev watchers
    if (stream->reading)
        ev_io_stop(EV_DEFAULT_ &stream->w_read);

    if (stream->writing)
        ev_io_stop(EV_DEFAULT_ &stream->w_write);

    close(stream->sd);

    free(stream);
}

int sev_listen(struct sev_server *server, const char *address, int port)
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

    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
        return -1;

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

struct sev_stream *sev_connect(const char *address, int port)
{
    int sd, rv;
    struct addrinfo *servinfo, *p;

    struct addrinfo hint = {};
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    char port_str[12];
    sprintf(port_str, "%d", port);

    if ((rv = getaddrinfo(address, port_str, &hint, &servinfo)) != 0)
        return NULL;

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sd);
            continue;
        }

        break;
    }

    if (p == NULL)
        return NULL;

    freeaddrinfo(servinfo);

    return sev_stream_new(sd, (struct sockaddr_in *)p->ai_addr);
}

void sev_loop(void)
{
    signal(SIGPIPE, SIG_IGN);

    ev_loop(EV_DEFAULT_ 0);
}

void sev_block_read(struct sev_stream *stream)
{
    if (stream->reading) {
        stream->reading = 0;
        ev_io_stop(EV_DEFAULT_ &stream->w_read);
    }
}

void sev_allow_read(struct sev_stream *stream)
{
    if (!stream->reading) {
        stream->reading = 1;
        ev_io_start(EV_DEFAULT_ &stream->w_read);
    }
}
