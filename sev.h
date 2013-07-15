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
