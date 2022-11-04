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

void list_insert(struct file_data* current, struct file_data* new) {
    new->next = current->next;
    current->next = new;
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

    // Remove trailing '/' from paths.
    if (source_path[strlen(source_path) - 1] == '/') 
        source_path[strlen(source_path) - 1] = '\0';

    if (destination_path[strlen(destination_path) - 1] == '/') 
        destination_path[strlen(destination_path) - 1] = '\0';

    if (isdir(source_path) == -1) return EXIT_FAILURE;
    if (isdir(destination_path) == -1) return EXIT_FAILURE;

    char* const path[2] = { source_path, NULL };
    file_hierarchy = fts_open(path, FTS_LOGICAL, NULL);
    if (errno) {
        perror(NULL);
        return EXIT_FAILURE;
    }

    struct file_data* head = NULL;
    struct file_data* previous = NULL;

    while ( (file = fts_read(file_hierarchy)) != NULL ) {
        if (file->fts_info == FTS_F) {
            struct file_data* current = malloc(sizeof *current);
            if (current == NULL) {
                perror(NULL);
                return EXIT_SUCCESS;
            }

            char const*const relative_path = file->fts_path + strlen(source_path);
            size_t relative_pathlen = strlen(relative_path) + 1; // including terminating '\0'

            current->path = malloc(relative_pathlen * sizeof *current->path);
            strncpy(current->path, relative_path, relative_pathlen);

            current->mtime = file->fts_statp->st_mtime;
            current->next = NULL;

            if (head == NULL) head = current;

            if (previous) list_insert(previous, current);
            previous = current;
        }
    }

    size_t destlen = strlen(destination_path);

    while (head) {
        printf("\n%s\n", head->path);
        time_t const* mtime = &head->mtime;
        printf("%s", ctime(mtime));

        size_t plen = strlen(head->path);
        char* path = malloc((destlen + plen + 1) * sizeof *path);

        strncpy(path, destination_path, destlen + 1);
        strncat(path, head->path, plen);
        printf("Path: %s\n", path);

        struct stat file_stat;

        if (stat(path, &file_stat) == -1) {
            if (errno == ENOENT) printf("Does not exist.\n");
            else {
                perror(NULL);
                return EXIT_FAILURE;
            }
        }

        struct file_data* tmp = head;
        head = tmp->next;
        free(tmp->path);
        free(tmp);
    }

    return EXIT_SUCCESS;
}
