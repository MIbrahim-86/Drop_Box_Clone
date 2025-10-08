#ifndef USER_AUTH_H
#define USER_AUTH_H

#define MAX_USERS 100
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_FILES 100
#define MAX_FILENAME_LEN 255
#define DEFAULT_QUOTA (10 * 1024 * 1024)

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
    pthread_mutex_t lock;
} User;

typedef struct {
    User users[MAX_USERS];
    int user_count;
    pthread_mutex_t lock;
} UserManager;

UserManager* user_manager_create();
void user_manager_destroy(UserManager *manager);
int user_signup(UserManager *manager, const char *username, const char *password);
int user_login(UserManager *manager, const char *username, const char *password);
User* get_user(UserManager *manager, const char *username);

#endif
