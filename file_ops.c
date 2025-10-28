#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include "common.h"

#define STORAGE_DIR "./user_files/"

void ensure_storage_dir() {
    struct stat st = {0};
    if (stat(STORAGE_DIR, &st) == -1) {
        mkdir(STORAGE_DIR, 0700);
    }
}

char* get_user_dir(const char *username) {
    static char path[512];
    snprintf(path, sizeof(path), "%s%s/", STORAGE_DIR, username);
    return path;
}

void ensure_user_dir(const char *username) {
    char path[512];
    snprintf(path, sizeof(path), "%s%s/", STORAGE_DIR, username);
    
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

char* get_file_path(const char *username, const char *filename) {
    static char path[512];
    snprintf(path, sizeof(path), "%s%s/%s", STORAGE_DIR, username, filename);
    return path;
}

void handle_upload(void *arg) {
    TaskData *data = (TaskData*)arg;
    if (!data) return;
    
    User *user = get_user(data->user_manager, data->username);
    if (!user) {
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: User not found\n", 22);
        free(data);
        return;
    }
    
    file_lock_acquire(data->lock_manager, data->username, data->filename);

    printf("DEBUG: UPLOAD acquired lock for %s. Sleeping for 5 seconds...\n", data->filename);
    sleep(5);
    
    pthread_mutex_lock(&user->lock);
    
    struct stat st;
    if (stat(data->local_path, &st) == 0) {
        if (user->used_quota + st.st_size > user->quota) {
            pthread_mutex_unlock(&user->lock);
            file_lock_release(data->lock_manager, data->username, data->filename);
            response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: Quota exceeded\n", 22);
            free(data);
            return;
        }
    } else {
        pthread_mutex_unlock(&user->lock);
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: Local file not found on server\n", 37);
        free(data);
        return;
    }
    
    pthread_mutex_unlock(&user->lock); 

    ensure_user_dir(data->username);
    char *server_path = get_file_path(data->username, data->filename);
    
    FILE *src = fopen(data->local_path, "rb");
    FILE *dst = fopen(server_path, "wb");
    
    if (!src || !dst) {
        if (src) fclose(src);
        if (dst) fclose(dst);
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: File operation failed\n", 29);
        free(data);
        return;
    }
    
    char buffer[4096];
    size_t bytes, total_bytes = 0;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
        total_bytes += bytes;
    }
    
    fclose(src);
    fclose(dst);
    
    pthread_mutex_lock(&user->lock);
    add_file_to_user(user, data->filename, total_bytes);
    user->used_quota += total_bytes;
    pthread_mutex_unlock(&user->lock);
    
    file_lock_release(data->lock_manager, data->username, data->filename);
    
    char response[512];
    snprintf(response, sizeof(response), "OK: Uploaded %s (%zu bytes)\n", 
             data->filename, total_bytes);
    response_queue_enqueue(data->response_queue, data->client_socket, response, (int)strlen(response));
    free(data);
}

void handle_download(void *arg) {
    TaskData *data = (TaskData*)arg;
    if (!data) return;
    
    User *user = get_user(data->user_manager, data->username);
    if (!user) {
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: User not found\n", 22);
        free(data);
        return;
    }

    file_lock_acquire(data->lock_manager, data->username, data->filename);
    pthread_mutex_lock(&user->lock);

    char *server_path = get_file_path(data->username, data->filename);
    
    int file_exists_in_meta = 0;
    for (int i = 0; i < user->file_count; i++) {
        if (strcmp(user->files[i].name, data->filename) == 0) {
            file_exists_in_meta = 1;
            break;
        }
    }
    
    if (!file_exists_in_meta) {
        pthread_mutex_unlock(&user->lock);
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: File not found in user metadata\n", 38);
        free(data);
        return;
    }
    
    pthread_mutex_unlock(&user->lock);

    FILE *file = fopen(server_path, "rb");
    if (!file) {
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: File not found on disk\n", 29);
        free(data);
        return;
    }
    
    response_queue_enqueue(data->response_queue, data->client_socket, "FILE_CONTENT\n", 13);
    
    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        response_queue_enqueue(data->response_queue, data->client_socket, buffer, (int)bytes);
    }
    
    fclose(file);
    response_queue_enqueue(data->response_queue, data->client_socket, "END_FILE\n", 9);
    
    file_lock_release(data->lock_manager, data->username, data->filename);
    free(data);
}

void handle_delete(void *arg) {
    TaskData *data = (TaskData*)arg;
    if (!data) return;
    
    User *user = get_user(data->user_manager, data->username);
    if (!user) {
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: User not found\n", 22);
        free(data);
        return;
    }
    
    file_lock_acquire(data->lock_manager, data->username, data->filename);
    pthread_mutex_lock(&user->lock);
    
    char *server_path = get_file_path(data->username, data->filename);
    
    if (remove(server_path) == 0) {

        for (int i = 0; i < user->file_count; i++) {
            if (strcmp(user->files[i].name, data->filename) == 0) {
                user->used_quota -= user->files[i].size;
                for (int j = i; j < user->file_count - 1; j++) {
                    user->files[j] = user->files[j + 1];
                }
                user->file_count--;
                break;
            }
        }
        
        pthread_mutex_unlock(&user->lock);
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "OK: File deleted\n", 17);
    } else {
        pthread_mutex_unlock(&user->lock);
        file_lock_release(data->lock_manager, data->username, data->filename);
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: File not found or delete failed\n", 38);
    }
    free(data);
}

void handle_list(void *arg) {
    TaskData *data = (TaskData*)arg;
    if (!data) return;
    
    User *user = get_user(data->user_manager, data->username);
    if (!user) {
        response_queue_enqueue(data->response_queue, data->client_socket, "ERROR: User not found\n", 22);
        free(data);
        return;
    }
    
    pthread_mutex_lock(&user->lock);
    
    char response[4096] = "Files:\n";
    int offset = (int)strlen(response);
    
    for (int i = 0; i < user->file_count; i++) {
        offset += snprintf(response + offset, sizeof(response) - offset,
                          "- %s (%zu bytes)\n", 
                          user->files[i].name, user->files[i].size);
    }
    
    offset += snprintf(response + offset, sizeof(response) - offset,
                      "Quota: %zu/%zu bytes used\n",
                      user->used_quota, user->quota);
    
    pthread_mutex_unlock(&user->lock);
    
    response_queue_enqueue(data->response_queue, data->client_socket, response, (int)strlen(response));
    free(data);
}

int add_file_to_user(User *user, const char *filename, size_t size) {
    for(int i = 0; i < user->file_count; i++) {
        if(strcmp(user->files[i].name, filename) == 0) {
            user->used_quota -= user->files[i].size;
            user->files[i].size = size;
            return 0;
        }
    }

    if (user->file_count >= MAX_FILES) return -1;
    
    strncpy(user->files[user->file_count].name, filename, MAX_FILENAME_LEN - 1);
    user->files[user->file_count].size = size;
    user->file_count++;
    
    return 0;
}

int remove_file_from_user(User *user, const char *filename) {
    for (int i = 0; i < user->file_count; i++) {
        if (strcmp(user->files[i].name, filename) == 0) {
            for (int j = i; j < user->file_count - 1; j++) {
                user->files[j] = user->files[j + 1];
            }
            user->file_count--;
            return 0;
        }
    }
    return -1;
}