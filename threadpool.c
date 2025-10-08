#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "threadpool.h"

static void* worker_thread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->lock);
        
        while (queue_is_empty(pool->task_queue) && !pool->shutdown) {
            pthread_cond_wait(&pool->condition, &pool->lock);
        }
        
        if (pool->shutdown && queue_is_empty(pool->task_queue)) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        
        Task *task = (Task*)queue_dequeue(pool->task_queue);
        pthread_mutex_unlock(&pool->lock);
        
        if (task) {
            task->function(task->argument);
            free(task);
        }
    }
    
    return NULL;
}

ThreadPool* threadpool_create(int thread_count, int queue_size) {
    ThreadPool *pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!pool) return NULL;
    
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
    
    pool->task_queue = queue_create(queue_size);
    if (!pool->task_queue) {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    pool->thread_count = thread_count;
    pool->shutdown = 0;
    
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->condition, NULL);
    
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
         
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->condition);
            
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            
            threadpool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

void threadpool_destroy(ThreadPool *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->condition);
    pthread_mutex_unlock(&pool->lock);
    
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    free(pool->threads);
    queue_destroy(pool->task_queue);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->condition);
    free(pool);
}

int threadpool_add_task(ThreadPool *pool, void (*function)(void *), void *argument) {
    if (!pool || pool->shutdown) return -1;
    
    Task *task = (Task*)malloc(sizeof(Task));
    if (!task) return -1;
    
    task->function = function;
    task->argument = argument;
    
    if (queue_enqueue(pool->task_queue, task) != 0) {
        free(task);
        return -1;
    }
    
    pthread_cond_signal(&pool->condition);
    return 0;
}
