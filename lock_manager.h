#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <pthread.h>

#define NUM_FILE_LOCKS 128 

typedef struct LockManager {
    pthread_mutex_t locks[NUM_FILE_LOCKS];
} LockManager;

LockManager* lock_manager_create();
void lock_manager_destroy(LockManager *manager);

void file_lock_acquire(LockManager *manager, const char *username, const char *filename);

void file_lock_release(LockManager *manager, const char *username, const char *filename);

#endif 
