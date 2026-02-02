#include "ringbuf.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ringbuf_t *ringbuf_create(size_t size)
{
    ringbuf_t *rb = malloc(sizeof(ringbuf_t));
    if(!rb)
        return NULL;

    rb->buffer = malloc(size);
    if(!rb->buffer){
        free(rb);
        return NULL;
    }
    if(pthread_mutex_init(&rb->mutex, NULL) != 0){
        free(rb->buffer);
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->full = false;

    return rb;
}

void ringbuf_destroy(ringbuf_t *rb)
{
    if(rb){
        pthread_mutex_destroy(&rb->mutex);
        free(rb->buffer);
        free(rb);
    }
}

static size_t ringbuf_free_space(const ringbuf_t *rb)
{
    if(rb->full)
        return 0;
    return (rb->head >= rb->tail) ? (rb->size - rb->head + rb->tail) : (rb->tail - rb->head);
}

static size_t ringbuf_used_space(const ringbuf_t *rb)
{
    return rb->full ? rb->size : (rb->head >= rb->tail) ? \
        rb->head - rb->tail : (rb->size - rb->tail + rb->head);
}

size_t ringbuf_write(ringbuf_t *rb, const void *data, size_t len)
{
    if(!rb || !data || len == 0)
        return 0;
    pthread_mutex_lock(&rb->mutex);
    size_t free_space = ringbuf_free_space(rb);
    if(len > free_space)
        len = free_space;
    if(len == 0){
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }
    const char *src = data;
    size_t first_chunk = rb->size - rb->head;
    if(first_chunk > len){
        memcpy(&rb->buffer[rb->head], src, len);
        rb->head = (rb->head + len) % rb->size;
    }
    else{
        memcpy(&rb->buffer[rb->head], src, first_chunk);
        memcpy(rb->buffer, src + first_chunk, len - first_chunk);
        rb->head = len - first_chunk;
    }
    if(rb->head == rb->tail)
        rb->full = true;

    pthread_mutex_unlock(&rb->mutex);
    return len;
}

size_t ringbuf_read(ringbuf_t *rb, void *data, size_t len)
{
    if(!rb || !data || len == 0)
        return 0;
    pthread_mutex_lock(&rb->mutex);
    size_t used = ringbuf_used_space(rb);
    if(len > used)
        len = used;
    if(len == 0){
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }

    unsigned char *dst = data;
    size_t first_chunk = rb->size - rb->tail;
    if(first_chunk >= len){
        memcpy(dst, &rb->buffer[rb->tail], len);
        rb->tail = (rb->tail + len) % rb->size;
    }
    else{
        memcpy(dst, &rb->buffer[rb->tail], first_chunk);
        memcpy(dst + first_chunk, rb->buffer, len - first_chunk);
        rb->tail = len - first_chunk;
    }
    rb->full = false;
    pthread_mutex_unlock(&rb->mutex);
    return len;
}

size_t ringbuf_read_try(ringbuf_t *rb, void *data, size_t len)
{
    if(!rb || !data || len == 0)
        return 0;
    pthread_mutex_lock(&rb->mutex);
    size_t used = ringbuf_used_space(rb);
    if(len > used)
        len = used;
    if(len == 0){
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }

    unsigned char *dst = data;
    size_t first_chunk = rb->size - rb->tail;
    if(first_chunk >= len){
        memcpy(dst, &rb->buffer[rb->tail], len);
        // rb->tail = (rb->tail + len) % rb->size;
    }
    else{
        memcpy(dst, &rb->buffer[rb->tail], first_chunk);
        memcpy(dst + first_chunk, rb->buffer, len - first_chunk);
        // rb->tail = len - first_chunk;
    }
    // rb->full = false;
    pthread_mutex_unlock(&rb->mutex);
    return len;
}

size_t ringbuf_get_free(ringbuf_t *rb)
{
    if(!rb)
        return 0;
    
    pthread_mutex_lock(&rb->mutex);
    size_t avail = ringbuf_free_space(rb);
    pthread_mutex_unlock(&rb->mutex);
    return avail;
}

size_t ringbuf_get_used(ringbuf_t *rb)
{
    if(!rb)
        return 0;

    pthread_mutex_lock(&rb->mutex);
    size_t used = ringbuf_used_space(rb);
    pthread_mutex_unlock(&rb->mutex);
    return used;
}

void ringbuf_discard(ringbuf_t *rb, size_t len)
{
    if(!rb || len == 0)
        return;
    pthread_mutex_lock(&rb->mutex);
    size_t used = ringbuf_used_space(rb);
    if(len > used)
        len = used;
    // rb->tail = (rb->tail + len) % rb->size;
    // rb->full = false;
    if(rb->full){
        if(len >= rb->size){
            rb->head = rb->tail = 0;
            rb->full = false;
        }
        else{
            rb->tail = (rb->tail + len) % rb->size;
            rb->full = false;
            rb->head = (rb->tail + used - len) % rb->size;
        }
    }
    else{
        rb->tail = (rb->tail + len) % rb->size;
    }

    pthread_mutex_unlock(&rb->mutex);
}

void ringbuf_cleanup(ringbuf_t *rb)
{
    if(!rb)
        return;
    pthread_mutex_lock(&rb->mutex);
    rb->tail = rb->head = 0;
    rb->full = false;
    pthread_mutex_unlock(&rb->mutex);
}

#ifdef __cplusplus
}
#endif