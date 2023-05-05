#pragma once

#include <stddef.h>

#ifdef DEBUG
#define debug_msg(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_msg(...) ((void) 42)
#endif

int aver(int return_status, char *fun_name);
int exists(char *path);
int is_directory(char *path);
size_t file_size(char *path);
