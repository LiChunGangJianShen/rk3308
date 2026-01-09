/******************************************************************************
 * Copyright (C) 2014-2020 Zhifeng Gong <gozfree@163.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "log.h"
#include "ringbuffer.h"

#define MIN(a, b)           ((a) > (b) ? (b) : (a))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define CALLOC(size, type)  (type *)calloc(size, sizeof(type))



struct ringbuffer *rb_create(int len)
{
    struct ringbuffer *rb = CALLOC(1, struct ringbuffer);
    if (!rb) {
        logw("malloc ringbuffer failed!\n");
        return NULL;
    }
    rb->length = len + 1;
    rb->start = 0;
    rb->end = 0;
    rb->buffer = calloc(1, rb->length);
    if (!rb->buffer) {
        logw("malloc rb->buffer failed!\n");
        free(rb);
        return NULL;
    }

    if (pthread_mutex_init(&rb->lock, NULL) != 0){
      	logw("mutex init fail\n");
   	}
    
    return rb;
}

void rb_destroy(struct ringbuffer *rb)
{
    if (!rb) {
        return;
    }
    pthread_mutex_destroy(&rb->lock);
    free(rb->buffer);
    free(rb);
}

void *rb_end_ptr(struct ringbuffer *rb)
{
    return (void *)((char *)rb->buffer + rb->end);
}

void *rb_start_ptr(struct ringbuffer *rb)
{
    return (void *)((char *)rb->buffer + rb->start);
}

static inline void rb_lock(struct ringbuffer *rb)
{
	if(rb) {
		pthread_mutex_lock(&rb->lock);
	}
}

static inline void rb_unlock(struct ringbuffer *rb)
{
	if(rb) {
		pthread_mutex_unlock(&rb->lock);
	}
}

static size_t rb_get_space_free_internal(struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }
    if (rb->end >= rb->start) {
        return rb->length - (rb->end - rb->start)-1;
    } else {
        return rb->start - rb->end-1;
    }
}

size_t rb_get_space_used_internal(struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }
    if (rb->end >= rb->start) {
        return rb->end - rb->start;
    } else {
        return rb->length - (rb->start - rb->end);
    }
}

size_t rb_get_space_free(struct ringbuffer *rb)
{
    size_t r;

    rb_lock(rb);
    r = rb_get_space_free_internal(rb);
    rb_unlock(rb);
    return r;
}

size_t rb_get_space_used(struct ringbuffer *rb)
{
    size_t r;

    rb_lock(rb);
    r = rb_get_space_used_internal(rb);
    rb_unlock(rb);
    return r;
}

ssize_t rb_write(struct ringbuffer *rb, const void *buf, size_t len)
{
    if (!rb) {
        return -1;
    }

    rb_lock(rb);

    size_t left = rb_get_space_free_internal(rb);
    if (len > left) {
        rb_unlock(rb);
        logw("Not enough space: %zu request, %zu available\n", len, left);
        return -1;
    }

    if ((rb->length - rb->end) < len) {
        int half_tail = rb->length - rb->end;
        memcpy(rb_end_ptr(rb), buf, half_tail);
        rb->end = (rb->end + half_tail) % rb->length;

        int half_head = len - half_tail;
        memcpy(rb_end_ptr(rb), buf+half_tail, half_head);
        rb->end = (rb->end + half_head) % rb->length;
    } else {
        memcpy(rb_end_ptr(rb), buf, len);
        rb->end = (rb->end + len) % rb->length;
    }

    rb_unlock(rb);
    return len;
}

ssize_t rb_read(struct ringbuffer *rb, void *buf, size_t len)
{
    if (!rb) {
        return -1;
    }

    rb_lock(rb);
    size_t rlen = MIN(len, rb_get_space_used_internal(rb));

    if ((rb->length - rb->start) < rlen) {
        int half_tail = rb->length - rb->start;
        memcpy(buf, rb_start_ptr(rb), half_tail);
        rb->start = (rb->start + half_tail) % rb->length;

        int half_head = rlen - half_tail;
        memcpy(buf+half_tail, rb_start_ptr(rb), half_head);
        rb->start = (rb->start + half_head) % rb->length;
    } else {
        memcpy(buf, rb_start_ptr(rb), rlen);
        rb->start = (rb->start + rlen) % rb->length;
    }

    if ((rb->start == rb->end) || (rb_get_space_used_internal(rb) == 0)) {
        rb->start = rb->end = 0;
    }

    rb_unlock(rb);
    return rlen;
}

ssize_t rb_read_try(struct ringbuffer *rb, void *buf, size_t len)
{
    if (!rb) {
        return -1;
    }

    rb_lock(rb);
    size_t rlen = MIN(len, rb_get_space_used_internal(rb));

    if ((rb->length - rb->start) < rlen) {
        int half_tail = rb->length - rb->start;
        memcpy(buf, rb_start_ptr(rb), half_tail);
        // rb->start = (rb->start + half_tail) % rb->length;

        int half_head = rlen - half_tail;
        memcpy(buf+half_tail, rb_start_ptr(rb), half_head);
        // rb->start = (rb->start + half_head) % rb->length;
    } else {
        memcpy(buf, rb_start_ptr(rb), rlen);
        // rb->start = (rb->start + rlen) % rb->length;
    }

    // if ((rb->start == rb->end) || (rb_get_space_used_internal(rb) == 0)) {
    //     rb->start = rb->end = 0;
    // }

    rb_unlock(rb);
    return rlen;
}

void rb_cleanup(struct ringbuffer *rb)
{
    if (!rb) {
        return;
    }
    rb_lock(rb);
    rb->start = rb->end = 0;
    rb_unlock(rb);
}


ssize_t rb_discard(struct ringbuffer *rb, size_t len)
{
    if (!rb) {
        return -1;
    }

    rb_lock(rb);
    size_t rlen = MIN(len, rb_get_space_used_internal(rb));

    if ((rb->length - rb->start) < rlen) {
        int half_tail = rb->length - rb->start;
        rb->start = (rb->start + half_tail) % rb->length;

        int half_head = rlen - half_tail;
        rb->start = (rb->start + half_head) % rb->length;
    } else {
        rb->start = (rb->start + rlen) % rb->length;
    }

    if ((rb->start == rb->end) || (rb_get_space_used_internal(rb) == 0)) {
        rb->start = rb->end = 0;
    }

    rb_unlock(rb);
    return rlen;
}

ssize_t rb_put_audio_1ch_16bit_1frame(struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }

    rb_lock(rb);

    size_t left = rb_get_space_free_internal(rb);
    char buf[2] = {0};
    size_t len = 2;
    int16_t *value, *p;

    if(left < 2) {
        memcpy(rb_end_ptr(rb), buf, len);
        rb->end = (rb->end + len) % rb->length;
    } else {
        if(rb->end >= 2) {
            value = (int16_t *)((char*)rb_end_ptr(rb)-2);
        } else {
            value = (int16_t *)((char *)rb->buffer + rb->length - 2);
        }

        p = (int16_t *)buf;
        *p = *value;

        if ((rb->length - rb->end) < len) {
            int half_tail = rb->length - rb->end;
            memcpy(rb_end_ptr(rb), buf, half_tail);
            rb->end = (rb->end + half_tail) % rb->length;

            int half_head = len - half_tail;
            memcpy(rb_end_ptr(rb), buf+half_tail, half_head);
            rb->end = (rb->end + half_head) % rb->length;
        } else {
            memcpy(rb_end_ptr(rb), buf, len);
            rb->end = (rb->end + len) % rb->length;
        }
    }

    rb_unlock(rb);
    return len;
}