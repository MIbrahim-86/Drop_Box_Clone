#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "queue.h"
#include <pthread.h>

typedef struct {
    void (*function)(void *);
    void *argument;
} Task;

typedef struct {
    pthread_t *threads;
    int thread_count;
    Queue *task_queue;
    volatile int shutdown;
    pthread_mutex_t lock;
    pthread_cond_t condition;
} ThreadPool;

ThreadPool* threadpool_create(int thread_count, int queue_size);
void threadpool_destroy(ThreadPool *pool);
int threadpool_add_task(ThreadPool *pool, void (*function)(void *), void *argument);

#endif
