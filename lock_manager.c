#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "lock_manager.h"

static unsigned long hash(const char *username, const char *filename) {
    char combined_key[512];
    snprintf(combined_key, sizeof(combined_key), "%s:%s", username, filename);
    
    unsigned long hash = 5381;
    int c;
    char *str = combined_key;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; 

    return hash;
}

LockManager* lock_manager_create() {
    LockManager *manager = (LockManager*)malloc(sizeof(LockManager));
    if (!manager) return NULL;
    
    for (int i = 0; i < NUM_FILE_LOCKS; i++) {
        pthread_mutex_init(&manager->locks[i], NULL);
    }
    return manager;
}

void lock_manager_destroy(LockManager *manager) {
    if (!manager) return;
    for (int i = 0; i < NUM_FILE_LOCKS; i++) {
        pthread_mutex_destroy(&manager->locks[i]);
    }
    free(manager);
}

static pthread_mutex_t* get_lock(LockManager *manager, const char *username, const char *filename) {
    unsigned long lock_index = hash(username, filename) % NUM_FILE_LOCKS;
    return &manager->locks[lock_index];
}

void file_lock_acquire(LockManager *manager, const char *username, const char *filename) {
    pthread_mutex_lock(get_lock(manager, username, filename));
}

void file_lock_release(LockManager *manager, const char *username, const char *filename) {
    pthread_mutex_unlock(get_lock(manager, username, filename));
}
