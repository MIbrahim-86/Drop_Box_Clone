#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

Queue* queue_create(int capacity) {
    Queue *queue = (Queue*)malloc(sizeof(Queue));
    if (!queue) return NULL;
    
    queue->items = (void**)malloc(sizeof(void*) * capacity);
    if (!queue->items) {
        free(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    
    return queue;
}

void queue_destroy(Queue *queue) {
    if (!queue) return;
    
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    
    free(queue->items);
    free(queue);
}

int queue_enqueue(Queue *queue, void *item) {
    if (!queue) return -1;
    
    pthread_mutex_lock(&queue->lock);
    
    while (queue->size == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->items[queue->rear] = item;
    queue->size++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    
    return 0;
}

void* queue_dequeue(Queue *queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->lock);
    
    while (queue->size == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    void *item = queue->items[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
    
    return item;
}

int queue_is_empty(Queue *queue) {
    if (!queue) return 1;
    pthread_mutex_lock(&queue->lock);
    int empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->lock);
    return empty;
}

int queue_is_full(Queue *queue) {
    if (!queue) return 0;
    pthread_mutex_lock(&queue->lock);
    int full = (queue->size == queue->capacity);
    pthread_mutex_unlock(&queue->lock);
    return full;
}
