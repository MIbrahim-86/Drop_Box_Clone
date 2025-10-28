#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

// Forward-declare to avoid circular dependencies
// Note: We forward-declare the *struct* names where possible
struct UserManager;
struct Queue;
struct ThreadPool;
struct LockManager; // This will be defined by a typedef, so we use the typedef name
struct ResponseQueue; // This will be an alias, so we use the typedef name

// --- From user_auth.h ---
#define MAX_FILES 100
#define MAX_FILENAME_LEN 255
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_USERS 100
#define DEFAULT_QUOTA 104857600 // 100MB

typedef struct {
    char name[MAX_FILENAME_LEN];
    size_t size;
} UserFile;

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    UserFile files[MAX_FILES];
    int file_count;
    size_t used_quota;
    size_t quota;
    pthread_mutex_t lock; // Per-user lock for metadata
} User;

typedef struct UserManager {
    User users[MAX_USERS];
    int user_count;
    pthread_mutex_t lock; // Lock for the manager itself
} UserManager;



typedef enum {
    OP_UPLOAD,
    OP_DOWNLOAD,
    OP_DELETE,
    OP_LIST
} OpType;


typedef struct Queue Queue;
typedef Queue ResponseQueue;
typedef struct LockManager LockManager;
typedef struct ThreadPool ThreadPool;

typedef struct {
    int client_socket;
    char username[MAX_USERNAME_LEN];
    OpType type;
    char filename[MAX_FILENAME_LEN];
    char local_path[MAX_FILENAME_LEN]; 
    
  
    UserManager *user_manager;
    ResponseQueue *response_queue;    
    LockManager *lock_manager;     
} TaskData;



typedef struct {
    int client_socket;
    char message[4096];
    int length;
} Response;



UserManager* user_manager_create();
void user_manager_destroy(UserManager *manager);
int user_signup(UserManager *manager, const char *username, const char *password);
int user_login(UserManager *manager, const char *username, const char *password);
User* get_user(UserManager *manager, const char *username);

void ensure_storage_dir();
char* get_user_dir(const char *username);
void ensure_user_dir(const char *username);
char* get_file_path(const char *username, const char *filename);
void handle_upload(void *arg);
void handle_download(void *arg);
void handle_delete(void *arg);
void handle_list(void *arg);
int add_file_to_user(User *user, const char *filename, size_t size);
int remove_file_from_user(User *user, const char *filename);


Queue* queue_create(int capacity);
void queue_destroy(Queue *queue);
int queue_enqueue(Queue *queue, void *item);
void* queue_dequeue(Queue *queue);
int queue_is_empty(Queue *queue);
int queue_is_full(Queue *queue);


ResponseQueue* response_queue_create(int capacity);
void response_queue_destroy(ResponseQueue *queue);
int response_queue_enqueue(ResponseQueue *queue, int client_socket, const char *message, int length);
void* response_queue_dequeue(ResponseQueue *queue); 


typedef struct {
    void (*function)(void *);
    void *argument;
} Task;
ThreadPool* threadpool_create(int thread_count, int queue_size);
void threadpool_destroy(ThreadPool *pool);
int threadpool_add_task(ThreadPool *pool, void (*function)(void *), void *argument);


struct LockManager {
    pthread_mutex_t locks[128]; 
};
LockManager* lock_manager_create();
void lock_manager_destroy(LockManager *manager);
void file_lock_acquire(LockManager *manager, const char *username, const char *filename);
void file_lock_release(LockManager *manager, const char *username, const char *filename);

#endif 