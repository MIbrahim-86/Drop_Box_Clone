#include <stdlib.h>
#include <string.h>
#include "common.h"

ResponseQueue* response_queue_create(int capacity) {
    return queue_create(capacity);
}

void response_queue_destroy(ResponseQueue *queue) {
    if (!queue) return;
    
    // Free any remaining items in the queue
    while (!queue_is_empty(queue)) {
        Response *res = (Response*)queue_dequeue(queue);
        if (res) {
            free(res);
        }
    }
    queue_destroy(queue);
}

int response_queue_enqueue(ResponseQueue *queue, int client_socket, const char *message, int length) {
    Response *res = (Response*)malloc(sizeof(Response));
    if (!res) return -1;
    
    res->client_socket = client_socket;

    if (length > sizeof(res->message)) {
        length = sizeof(res->message); 
    }
    
    memcpy(res->message, message, length);
    res->length = length;

    if (length < sizeof(res->message)) { // <-- ADD THIS IF BLOCK
        res->message[length] = '\0';      // Add null terminator
    } else {
        res->message[sizeof(res->message) - 1] = '\0'; // Ensure null term anyway
    }

    if (queue_enqueue(queue, res) != 0) {
        free(res);
        return -1;
    }
    return 0;
}

void* response_queue_dequeue(ResponseQueue *queue) {
    return queue_dequeue(queue); // Returns Response*
}