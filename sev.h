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

#ifndef SEV_H
#define SEV_H

#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>

#define RECV_BUFFER_SIZE 2048 // fits a 1500-byte MTU packet
#define SEND_BUFFER_SIZE 4096

struct sev_stream;

typedef void (sev_open_cb)(struct sev_stream *stream);
typedef void (sev_read_cb)(struct sev_stream *stream, char *data, size_t len);
typedef void (sev_close_cb)(struct sev_stream *stream, const char *reason);

struct sev_server {
    // socket descriptor
    int sd;

    // libev watcher
    struct ev_io watcher;

    // callbacks
    sev_open_cb *open_cb;
    sev_read_cb *read_cb;
    sev_close_cb *close_cb;

    // user data
    void *data;
};

struct sev_stream {
    // socket descriptor
    int sd;

    // libev watchers
    struct ev_io w_read;
    struct ev_io w_write;
    int writing;

    // callbacks
    sev_open_cb *open_cb;
    sev_read_cb *read_cb;
    sev_close_cb *close_cb;

    // stream info
    char remote_address[INET_ADDRSTRLEN];
    int remote_port;

    struct sev_server *server;

    // user data
    void *data;

    // circular buffer
    char buffer[SEND_BUFFER_SIZE];
    size_t buffer_start;
    size_t buffer_end;
    size_t buffer_len;
};

int sev_send(struct sev_stream *stream, const char *data, size_t len);

void sev_close(struct sev_stream *stream, const char *reason);

int sev_listen(struct sev_server *server, const char *address, int port);

struct sev_stream *sev_connect(const char *address, int port);

void sev_loop(void);

#endif
