#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h> 
#include <sys/types.h> 
#include "common.h" 

#define SERVER_PORT 8080
#define CLIENT_QUEUE_SIZE 100
#define TASK_QUEUE_SIZE 100
#define CLIENT_THREADS 5
#define WORKER_THREADS 5


ResponseQueue *response_queue = NULL;
LockManager *lock_manager = NULL;
pthread_t response_thread;
Queue *client_queue = NULL;
ThreadPool *client_pool = NULL;
ThreadPool *worker_pool = NULL;
UserManager *user_manager = NULL;
volatile int server_running = 1;
int server_fd; 

typedef struct {
    int client_socket;
    UserManager *user_manager;
    ThreadPool *worker_pool;
} ClientHandlerData;


void handle_signal(int sig) {
    
    server_running = 0;
    
    shutdown(server_fd, SHUT_RDWR); 
}


void* response_sender_thread(void *arg) {
    ResponseQueue *queue = (ResponseQueue*)arg;
    printf("Response sender thread started.\n");
    
    while (1) { 
        Response *res = (Response*)response_queue_dequeue(queue);
        if (!res) {
            if (!server_running) break; 
            continue;
        }

        if (res->client_socket == -1 && strcmp(res->message, "SHUTDOWN") == 0) {
            printf("Response thread shutting down.\n");
            free(res);
            break; 
        }

        if (res->length > 0) {
            send(res->client_socket, res->message, res->length, 0);
        }
        
        free(res);
    }
    return NULL;
}


void* client_handler(void *arg) {
    ClientHandlerData *data = (ClientHandlerData*)arg;
    int client_socket = data->client_socket;
    UserManager *user_manager = data->user_manager;
    ThreadPool *worker_pool = data->worker_pool;
    
    char buffer[1024];
    char username[MAX_USERNAME_LEN] = "";
    int authenticated = 0;
    
    fd_set read_fds;
    struct timeval tv;
    
    while (!authenticated && server_running) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(client_socket + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0) {
            perror("select error");
            break; 
        }
        
        if (activity == 0) {
            continue;
        }

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
                    response_queue_enqueue(response_queue, client_socket, "OK: Signup successful\n", 22);
                } else {
                    response_queue_enqueue(response_queue, client_socket, "ERROR: Signup failed\n", 21);
                }
            }
        }
        else if (strncmp(buffer, "LOGIN ", 6) == 0) {
            char user[50], pass[50];
            if (sscanf(buffer + 6, "%49s %49s", user, pass) == 2) {
                if (user_login(user_manager, user, pass) == 0) {
                    strncpy(username, user, sizeof(username) - 1);
                    authenticated = 1;
                    response_queue_enqueue(response_queue, client_socket, "OK: Login successful\n", 21);
                } else {
                    response_queue_enqueue(response_queue, client_socket, "ERROR: Login failed\n", 20);
                }
            }
        }
        else {
             response_queue_enqueue(response_queue, client_socket, "ERROR: Please login or signup first\n", 36);
        }
    }
    
    while (authenticated && server_running) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(client_socket + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("select error");
            break;
        }

        if (activity == 0) {
            continue;
        }
        
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            break; 
        }
        
        buffer[bytes_received] = '\0';
        printf("Socket %d received command: %s", client_socket, buffer);
        
        TaskData *task_data = (TaskData*)malloc(sizeof(TaskData));
        if (!task_data) continue;
        
        strncpy(task_data->username, username, sizeof(task_data->username) - 1);
        task_data->client_socket = client_socket;
        task_data->user_manager = user_manager;
        task_data->response_queue = response_queue;
        task_data->lock_manager = lock_manager;
        
        if (strncmp(buffer, "UPLOAD ", 7) == 0) {
            char filename[255], local_path[255], extra_arg[255]; 
            
            int items_scanned = sscanf(buffer + 7, "%254s %254s %254s", filename, local_path, extra_arg);
            
            if (items_scanned == 2) { 
                task_data->type = OP_UPLOAD;
                strncpy(task_data->filename, filename, sizeof(task_data->filename) - 1);
                strncpy(task_data->local_path, local_path, sizeof(task_data->local_path) - 1);
                threadpool_add_task(worker_pool, handle_upload, task_data);
            } else {
                response_queue_enqueue(response_queue, client_socket, "ERROR: Invalid UPLOAD command. Use: UPLOAD <remote_name> <local_name>\n", 65);
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
                response_queue_enqueue(response_queue, client_socket, "ERROR: Invalid DOWNLOAD command\n", 32);
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
                response_queue_enqueue(response_queue, client_socket, "ERROR: Invalid DELETE command\n", 30);
                free(task_data);
            }
        }
        else if (strncmp(buffer, "LIST", 4) == 0) {
            task_data->type = OP_LIST;
            threadpool_add_task(worker_pool, handle_list, task_data);
        }
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            free(task_data);
            response_queue_enqueue(response_queue, client_socket, "OK: Goodbye\n", 12);
            break;
        }
        else {
            response_queue_enqueue(response_queue, client_socket, "ERROR: Unknown command\n", 23);
            free(task_data);
        }
    }
    
    close(client_socket);
    printf("Client %d disconnected.\n", client_socket);
    free(data);
    return NULL;
}

void* accept_connections(void *arg) {
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
        
        printf("New connection accepted (socket %d)\n", client_socket);
        
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
    
    printf("Accept thread stopping.\n");
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

   response_queue = response_queue_create(100);
   if (!response_queue) {
        fprintf(stderr, "Failed to create response queue\n");
        return 1;
   }
   
   lock_manager = lock_manager_create();
   if (!lock_manager) {
        fprintf(stderr, "Failed to create lock manager\n");
        return 1;
   }
    
    pthread_create(&response_thread, NULL, response_sender_thread, response_queue);
    
    ensure_storage_dir();
    
    printf("Dropbox Server Starting...\n");
    printf("Client threads: %d\n", CLIENT_THREADS);
    printf("Worker threads: %d\n", WORKER_THREADS);
    
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_connections, NULL) != 0) {
        fprintf(stderr, "Failed to create accept thread\n");
        return 1;
    }
    
    pthread_join(accept_thread, NULL);
    
    printf("\nCleaning up resources...\n");
    
    printf("Destroying client pool...\n");
    threadpool_destroy(client_pool);
    printf("Destroying worker pool...\n");
    threadpool_destroy(worker_pool);
    
    printf("Signaling response thread...\n");
    response_queue_enqueue(response_queue, -1, "SHUTDOWN", 8);
    pthread_join(response_thread, NULL);
    
    printf("Destroying queues and managers...\n");
    if (response_queue) response_queue_destroy(response_queue);
    if (lock_manager) lock_manager_destroy(lock_manager);
    if (client_queue) queue_destroy(client_queue); 
    if (user_manager) user_manager_destroy(user_manager);
    
    printf("Server shutdown complete\n");
	return 0;
}
