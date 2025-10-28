#ifndef USER_AUTH_H
#define USER_AUTH_H

struct UserManager;
struct User;

UserManager* user_manager_create();
void user_manager_destroy(struct UserManager *manager);
int user_signup(struct UserManager *manager, const char *username, const char *password);
int user_login(struct UserManager *manager, const char *username, const char *password);
struct User* get_user(struct UserManager *manager, const char *username);

#endif