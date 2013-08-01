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

#include <stdio.h>
#include <string.h>
#include "../sev.h"

#define ADDRESS "lessandro.com"
#define PORT 80

static void read_cb(struct sev_stream *stream, char *data, size_t len)
{
    data[len] = '\0';

    printf("read %s: %s\n", stream->remote_address, data);
}

static void close_cb(struct sev_stream *stream, const char *reason)
{
    printf("close %s %s\n", stream->remote_address, reason);
}

static const char request[] =
    "GET /secret.txt HTTP/1.0\r\n"
    "Host: lessandro.com\r\n"
    "Connection: close\r\n"
    "\r\n";

int main(int argc, char *argv[])
{
    printf("connecting to %s:%d...\n", ADDRESS, PORT);

    struct sev_stream *stream = sev_connect(ADDRESS, PORT);

    if (!stream) {
        perror("sev_connect");
        return -1;
    }

    stream->read_cb = read_cb;
    stream->close_cb = close_cb;
    stream->data = NULL;

    printf("connected. sending request...\n");
    sev_send(stream, request, strlen(request));

    sev_loop();

    return 0;
}
