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

#include <stdlib.h>
#include <string.h>
#include "sev_queue.h"

static struct sev_buffer *sev_buffer_new(const char *data, size_t len)
{
    struct sev_buffer *buffer = malloc(sizeof(struct sev_buffer));
    buffer->start = 0;
    buffer->len = len;
    buffer->data = malloc(len);
    memcpy(buffer->data, data, len);
    return buffer;
}

static void sev_buffer_free(struct sev_buffer *buffer)
{
    free(buffer->data);
    free(buffer);
}

struct sev_queue *sev_queue_new(void)
{
    struct sev_queue *queue = malloc(sizeof(struct sev_queue));
    STAILQ_INIT(&queue->head);
    return queue;
}

void sev_queue_free(struct sev_queue *queue)
{
    struct sev_buffer *buffer = STAILQ_FIRST(&queue->head);

    while (buffer != NULL) {
        struct sev_buffer *next = STAILQ_NEXT(buffer, entries);
        sev_buffer_free(buffer);
        buffer = next;
    }
    STAILQ_INIT(&queue->head);

    free(queue);
}

struct sev_buffer *sev_queue_head(struct sev_queue *queue)
{
    return STAILQ_FIRST(&queue->head);
}

void sev_queue_free_head(struct sev_queue *queue)
{
    struct sev_buffer *buffer = sev_queue_head(queue);
    STAILQ_REMOVE_HEAD(&queue->head, entries);
    sev_buffer_free(buffer);
}

void sev_queue_push_back(struct sev_queue *queue, const char *data, size_t len)
{
    struct sev_buffer *buffer = sev_buffer_new(data, len);
    STAILQ_INSERT_TAIL(&queue->head, buffer, entries);
}
