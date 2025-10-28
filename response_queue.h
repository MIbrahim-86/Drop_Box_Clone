#ifndef RESPONSE_QUEUE_H
#define RESPONSE_QUEUE_H

#include "common.h" // Includes queue.h definition

// We just need a wrapper around the generic queue
// Note: typedef is now in common.h
ResponseQueue* response_queue_create(int capacity);
void response_queue_destroy(ResponseQueue *queue);
int response_queue_enqueue(ResponseQueue *queue, int client_socket, const char *message, int length);
void* response_queue_dequeue(ResponseQueue *queue); // Returns Response*

#endif // RESPONSE_QUEUE_H