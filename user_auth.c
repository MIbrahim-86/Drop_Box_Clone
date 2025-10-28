#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "common.h"

UserManager* user_manager_create() {
    UserManager *manager = (UserManager*)malloc(sizeof(UserManager));
    if (!manager) return NULL;
    
    manager->user_count = 0;
    pthread_mutex_init(&manager->lock, NULL);
    
    return manager;
}

void user_manager_destroy(UserManager *manager) {
    if (!manager) return;
    
    for (int i = 0; i < manager->user_count; i++) {
        pthread_mutex_destroy(&manager->users[i].lock);
    }
    
    pthread_mutex_destroy(&manager->lock);
    free(manager);
}

int user_signup(UserManager *manager, const char *username, const char *password) {
    if (!manager || !username || !password) return -1;
    
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->user_count; i++) {
        if (strcmp(manager->users[i].username, username) == 0) {
            pthread_mutex_unlock(&manager->lock);
            return -1;
        }
    }
    
    if (manager->user_count >= MAX_USERS) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    User *new_user = &manager->users[manager->user_count];
    strncpy(new_user->username, username, MAX_USERNAME_LEN - 1);
    new_user->username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(new_user->password, password, MAX_PASSWORD_LEN - 1);
    new_user->password[MAX_PASSWORD_LEN - 1] = '\0';
    new_user->file_count = 0;
    new_user->used_quota = 0;
    new_user->quota = DEFAULT_QUOTA;
    pthread_mutex_init(&new_user->lock, NULL);
    
    manager->user_count++;
    pthread_mutex_unlock(&manager->lock);
    
    return 0;
}

int user_login(UserManager *manager, const char *username, const char *password) {
    if (!manager || !username || !password) return -1;
    
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->user_count; i++) {
        if (strcmp(manager->users[i].username, username) == 0 &&
            strcmp(manager->users[i].password, password) == 0) {
            pthread_mutex_unlock(&manager->lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return -1;
}

User* get_user(UserManager *manager, const char *username) {
    if (!manager || !username) return NULL;
    
    pthread_mutex_lock(&manager->lock);
    
    for (int i = 0; i < manager->user_count; i++) {
        if (strcmp(manager->users[i].username, username) == 0) {
            pthread_mutex_unlock(&manager->lock);
            return &manager->users[i];
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return NULL;
}