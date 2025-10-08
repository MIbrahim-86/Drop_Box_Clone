#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct {
    void **items;
    int capacity;
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Queue;

Queue* queue_create(int capacity);
void queue_destroy(Queue *queue);
int queue_enqueue(Queue *queue, void *item);
void* queue_dequeue(Queue *queue);
int queue_is_empty(Queue *queue);
int queue_is_full(Queue *queue);

#endif
