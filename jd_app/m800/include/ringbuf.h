#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ringbuf {
    unsigned char *buffer;
    size_t size;
    size_t head;
    size_t tail;
    bool full;
    pthread_mutex_t mutex;
} ringbuf_t;

ringbuf_t *ringbuf_create(size_t size);
void ringbuf_destroy(ringbuf_t *rb);
size_t ringbuf_write(ringbuf_t *rb, const void *data, size_t len);
size_t ringbuf_read(ringbuf_t *rb, void *data, size_t len);
size_t ringbuf_read_try(ringbuf_t *rb, void *data, size_t len);
size_t ringbuf_get_free(ringbuf_t *rb);
size_t ringbuf_get_used(ringbuf_t *rb);
void ringbuf_discard(ringbuf_t *rb, size_t len);
void ringbuf_cleanup(ringbuf_t *rb);

#ifdef __cplusplus
}
#endif
#endif