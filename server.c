
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "queue.h"
#include "threadpool.h"
#include "user_auth.h"
#include "file_ops.h"

#define SERVER_PORT 8080
#define CLIENT_QUEUE_SIZE 100
#define TASK_QUEUE_SIZE 100
#define CLIENT_THREADS 5
#define WORKER_THREADS 5

typedef struct {
    int client_socket;
    UserManager *user_manager;
    ThreadPool *worker_pool;
} ClientHandlerData;

Queue *client_queue = NULL;
ThreadPool *client_pool = NULL;
ThreadPool *worker_pool = NULL;
UserManager *user_manager = NULL;
volatile int server_running = 1;

void handle_signal(int sig) {
    printf("\nShutting down server...\n");
    server_running = 0;
}

void* client_handler(void *arg) {
    ClientHandlerData *data = (ClientHandlerData*)arg;
    int client_socket = data->client_socket;
    UserManager *user_manager = data->user_manager;
    ThreadPool *worker_pool = data->worker_pool;
    
    char buffer[1024];
    char username[MAX_USERNAME_LEN] = "";
    int authenticated = 0;
    
    while (!authenticated && server_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        if (strncmp(buffer, "SIGNUP ", 7) == 0) {
            char user[50], pass[50];
            if (sscanf(buffer + 7, "%49s %49s", user, pass) == 2) {
                if (user_signup(user_manager, user, pass) == 0) {
                    send(client_socket, "OK: Signup successful\n", 22, 0);
                } else {
                    send(client_socket, "ERROR: Signup failed\n", 21, 0);
                }
            }
        }
        else if (strncmp(buffer, "LOGIN ", 6) == 0) {
            char user[50], pass[50];
            if (sscanf(buffer + 6, "%49s %49s", user, pass) == 2) {
                if (user_login(user_manager, user, pass) == 0) {
                    strncpy(username, user, sizeof(username) - 1);
                    authenticated = 1;
                    send(client_socket, "OK: Login successful\n", 21, 0);
                } else {
                    send(client_socket, "ERROR: Login failed\n", 20, 0);
                }
            }
        }
        else {
            send(client_socket, "ERROR: Please login or signup first\n", 36, 0);
        }
    }
    
    while (authenticated && server_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("Received command: %s", buffer);
        
        TaskData *task_data = (TaskData*)malloc(sizeof(TaskData));
        if (!task_data) continue;
        
        strncpy(task_data->username, username, sizeof(task_data->username) - 1);
        task_data->client_socket = client_socket;
        task_data->user_manager = user_manager;
        
        if (strncmp(buffer, "UPLOAD ", 7) == 0) {
            char filename[255], local_path[255];
            if (sscanf(buffer + 7, "%254s %254s", filename, local_path) == 2) {
                task_data->type = OP_UPLOAD;
                strncpy(task_data->filename, filename, sizeof(task_data->filename) - 1);
                strncpy(task_data->local_path, local_path, sizeof(task_data->local_path) - 1);
                threadpool_add_task(worker_pool, handle_upload, task_data);
            } else {
                send(client_socket, "ERROR: Invalid UPLOAD command\n", 30, 0);
                free(task_data);
            }
        }
        else if (strncmp(buffer, "DOWNLOAD ", 9) == 0) {
            char filename[255];
            if (sscanf(buffer + 9, "%254s", filename) == 1) {
                task_data->type = OP_DOWNLOAD;
                strncpy(task_data->filename, filename, sizeof(task_data->filename) - 1);
                threadpool_add_task(worker_pool, handle_download, task_data);
            } else {
                send(client_socket, "ERROR: Invalid DOWNLOAD command\n", 32, 0);
                free(task_data);
            }
        }
        else if (strncmp(buffer, "DELETE ", 7) == 0) {
            char filename[255];
            if (sscanf(buffer + 7, "%254s", filename) == 1) {
                task_data->type = OP_DELETE;
                strncpy(task_data->filename, filename, sizeof(task_data->filename) - 1);
                threadpool_add_task(worker_pool, handle_delete, task_data);
            } else {
                send(client_socket, "ERROR: Invalid DELETE command\n", 30, 0);
                free(task_data);
            }
        }
        else if (strncmp(buffer, "LIST", 4) == 0) {
            task_data->type = OP_LIST;
            threadpool_add_task(worker_pool, handle_list, task_data);
        }
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            send(client_socket, "OK: Goodbye\n", 12, 0);
            break;
        }
        else {
            send(client_socket, "ERROR: Unknown command\n", 23, 0);
            free(task_data);
        }
    }
    
    close(client_socket);
    free(data);
    return NULL;
}

void* accept_connections(void *arg) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", SERVER_PORT);
    
    while (server_running) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, 
                                 (socklen_t*)&addrlen);
        
        if (client_socket < 0) {
            if (server_running) {
                perror("accept");
            }
            continue;
        }
        
        printf("New connection accepted\n");
        
        ClientHandlerData *handler_data = (ClientHandlerData*)malloc(sizeof(ClientHandlerData));
        if (!handler_data) {
            close(client_socket);
            continue;
        }
        
        handler_data->client_socket = client_socket;
        handler_data->user_manager = user_manager;
        handler_data->worker_pool = worker_pool;
        
        threadpool_add_task(client_pool, (void (*)(void *))client_handler, handler_data);
    }
    
    close(server_fd);
    return NULL;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    user_manager = user_manager_create();
    if (!user_manager) {
        fprintf(stderr, "Failed to create user manager\n");
        return 1;
    }
    
    client_queue = queue_create(CLIENT_QUEUE_SIZE);
    if (!client_queue) {
        fprintf(stderr, "Failed to create client queue\n");
        return 1;
    }
    
    client_pool = threadpool_create(CLIENT_THREADS, CLIENT_QUEUE_SIZE);
    if (!client_pool) {
        fprintf(stderr, "Failed to create client thread pool\n");
        return 1;
    }
    
    worker_pool = threadpool_create(WORKER_THREADS, TASK_QUEUE_SIZE);
    if (!worker_pool) {
        fprintf(stderr, "Failed to create worker thread pool\n");
        return 1;
    }
    
    ensure_storage_dir();
    
    printf("Dropbox Server Starting...\n");
    printf("Client threads: %d\n", CLIENT_THREADS);
    printf("Worker threads: %d\n", WORKER_THREADS);
    
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_connections, NULL) != 0) {
        fprintf(stderr, "Failed to create accept thread\n");
        return 1;
    }
    
    while (server_running) {
        sleep(1);
    }
    
    printf("Cleaning up...\n");
    pthread_join(accept_thread, NULL);
    threadpool_destroy(client_pool);
    threadpool_destroy(worker_pool);
    queue_destroy(client_queue);
    user_manager_destroy(user_manager);
    
    printf("Server shutdown complete\n");
    return 0;
}
