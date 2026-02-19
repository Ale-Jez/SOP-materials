#include "common.h"

void add_manage(char** args, children_data_t* child_data, int argCount);
void end_manage(char** args, children_data_t* child_data, int argCount);
void list_manage(char** args, children_data_t* child_data);
void restore_manage(char** args, children_data_t* child_data, int argCount);

int exit_manage(char** args);

//
// helpers for backups
int is_subpath(const char* parent, const char* child);

int is_dir_empty(const char* path);
void copy_item(const char* src_path, const char* dst_path, const char* root_src, const char* root_dst);
void remove_recursive(const char* path);
void copy_recursive(const char* src_base, const char* dst_base, const char* root_src, const char* root_dst);
