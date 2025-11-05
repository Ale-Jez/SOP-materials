#define _XOPEN_SOURCE 700
#include "utils.h"

void set(char* user, char* path, char* value) { printf("%s SET %s to %s\n", user, path, value); }

void get(char* user, char* path) { printf("%s GET %s\n", user, path); }

void share(char* user, char* path, char* other_user) { printf("%s SHARE %s with %s\n", user, path, other_user); }

void erase(char* user, char* path) { printf("%s ERASE %s\n", user, path); }

int main(int argc, char** argv) {}
