#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "user_auth.h"

typedef enum {
    OP_UPLOAD,
    OP_DOWNLOAD,
    OP_DELETE,
    OP_LIST
} OperationType;

typedef struct {
    OperationType type;
    char username[MAX_USERNAME_LEN];
    char filename[MAX_FILENAME_LEN];
    char local_path[MAX_FILENAME_LEN];
    int client_socket;
    UserManager *user_manager;
} TaskData;

void handle_upload(void *arg);
void handle_download(void *arg);
void handle_delete(void *arg);
void handle_list(void *arg);

int add_file_to_user(User *user, const char *filename, size_t size);
int remove_file_from_user(User *user, const char *filename);

void ensure_storage_dir(void);

#endif
