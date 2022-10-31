#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define VERSION "0.1"

void print_usage() {
    printf("gonc " VERSION "\n");
}

int isdir(char const*const path) {
    struct stat file_stat;

    if (stat(path, &file_stat) == -1) {
        perror(NULL);
        return -1;
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        printf("'%s' must be a directory.\n", path);
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[argc+1]) {
    char const* source_path;
    char const* destination_path;

    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    source_path = argv[1];
    destination_path = argv[2];

    if (isdir(source_path) == -1) return EXIT_FAILURE;
    if (isdir(destination_path) == -1) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
