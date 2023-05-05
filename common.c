#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

int64_t aver(int64_t return_status, char *fun_name) {
    if (return_status < 0) {
        fprintf(stderr, "error %s: %s\n", fun_name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return return_status;
}

int exists(char *path) {
    return access(path, F_OK) == 0;
}

int is_directory(char *path) {
    assert(exists(path));
    struct stat stat_buf;
    assert(stat(path, &stat_buf) == 0);
    return S_ISDIR(stat_buf.st_mode);
}

size_t file_size(char *path) {
    assert(exists(path));
    struct stat stat_buf;
    assert(stat(path, &stat_buf) == 0);
    return stat_buf.st_size;
}

