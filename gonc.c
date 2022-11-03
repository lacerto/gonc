#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>

#define VERSION "0.1"

struct file_data {
    char* path;
    time_t mtime;
    struct file_data* next;
};

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
    FTS* file_hierarchy;
    FTSENT* file;

    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    char* const source_path = argv[1];
    char* const destination_path = argv[2];

    // Remove trailing '/' from source path if there is one.
    if (source_path[strlen(source_path) - 1] == '/') 
        source_path[strlen(source_path) - 1] = '\0';

    if (isdir(source_path) == -1) return EXIT_FAILURE;
    if (isdir(destination_path) == -1) return EXIT_FAILURE;

    char* const path[2] = { source_path, NULL };
    file_hierarchy = fts_open(path, FTS_LOGICAL, NULL);
    if (errno) {
        perror(NULL);
        return EXIT_FAILURE;
    }

    while ( (file = fts_read(file_hierarchy)) != NULL ) {
        if (file->fts_info == FTS_F) {
            printf("path: %s\nname: %s\n", file->fts_path, file->fts_name);
            printf("pathlen: %i\nnamelen: %i\n", file->fts_pathlen, file->fts_namelen);
            time_t const* mtime = &file->fts_statp->st_mtime;
            printf("%s", ctime(mtime));

            struct file_data* head = malloc(sizeof *head);
            if (head == NULL) {
                perror(NULL);
                return EXIT_SUCCESS;
            }
            head->path = malloc((file->fts_pathlen + 1) * sizeof *head->path);
            strncpy(head->path, file->fts_path, file->fts_pathlen + 1);
            head->mtime = file->fts_statp->st_mtime;
            head->next = NULL;
            printf("%s\n", head->path);
            free(head->path);
            free(head);
        }
    }

    return EXIT_SUCCESS;
}
