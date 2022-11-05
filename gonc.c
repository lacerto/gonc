#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>

#define VERSION "0.2 (2022-11-05)"

struct file_data {
    char* path;
    time_t mtime;
    struct file_data* next;
};

void print_version() {
    printf("This is gonc version " VERSION ".\n");
}

/*
 * Prints usage information.
 */
void print_usage() {
    printf("\nSynchronizes gopher directories.\n\n");
    printf("Usage:\n");
    printf("\tgonc source_dir destination_dir\n");
    printf("\tgonc -h\n");
    printf("\tgonc -v\n");
    printf("\nOptions:\n");
    printf("\t-h\tShow this help.\n");
    printf("\t-v\tShow the version number.\n");
}

bool isdir(char const*const path) {
    struct stat file_stat;

    if (stat(path, &file_stat) == -1) {
        perror(path);
        return false;
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        printf("'%s' must be a directory.\n", path);
        return false;
    }
    return true;
}

void remove_trailing_slash(char* const path) {
    size_t length = strlen(path);
    if (length == 0) return;

    char* p = path + (length -1);
    if (*p == '/') *p = '\0';
}

struct file_data* get_sentinel_node() {
    struct file_data* sentinel = malloc(sizeof *sentinel);
    if (sentinel == NULL) {
        perror("Failed to allocate memory for list head");
        exit(EXIT_FAILURE);
    } else {
        sentinel->path = NULL;
        sentinel->mtime = 0;
        sentinel->path = NULL;
        return sentinel;
    }
}

void list_insert(struct file_data* restrict current, struct file_data* restrict new) {
    new->next = current->next;
    current->next = new;
}

void free_list(struct file_data* restrict head) {
    struct file_data* current = head->next;
    struct file_data* next;
    while (current) {
        next = current->next;
        free(current->path);
        free(current);
        current = next;
    }
    free(head); // free the sentinel
}

void get_file_list(struct file_data* head, char* const restrict dirpath) {
    FTS* file_hierarchy;
    FTSENT* file;

    char* const path[2] = { dirpath, NULL };
    file_hierarchy = fts_open(path, FTS_LOGICAL, NULL);
    if (errno) {
        perror("Could not open file hierarchy");
        exit(EXIT_FAILURE);
    }

    struct file_data* previous = head;

    while ( (file = fts_read(file_hierarchy)) != NULL ) {
        if (file->fts_info == FTS_F) {
            struct file_data* current = malloc(sizeof *current);
            if (current == NULL) {
                perror("Could not allocate memory for file data");
                exit(EXIT_SUCCESS);
            }

            char const*const relative_path = file->fts_path + strlen(dirpath);
            size_t relative_pathlen = strlen(relative_path) + 1; // including terminating '\0'

            current->path = malloc(relative_pathlen * sizeof *current->path);
            strncpy(current->path, relative_path, relative_pathlen);

            current->mtime = file->fts_statp->st_mtime;
            current->next = NULL;

            list_insert(previous, current);
            previous = current;
        }
    }
}

int main(int argc, char* argv[argc+1]) {

    switch (argc) {
        case 3:
            print_version();
            break;
        case 2:
            if (strncmp(argv[1], "-h", 2) == 0) {
                print_usage();
                return EXIT_SUCCESS;
            } else if (strncmp(argv[1], "-v", 2) == 0) {
                print_version();
                return EXIT_SUCCESS;
            }
            // falls through
        default:
            print_usage();
            return EXIT_FAILURE;
    }

    char* const source_path = argv[1];
    char* const destination_path = argv[2];

    remove_trailing_slash(source_path);
    remove_trailing_slash(destination_path);

    if (!isdir(source_path)) return EXIT_FAILURE;
    if (!isdir(destination_path)) return EXIT_FAILURE;

    struct file_data* src_head = get_sentinel_node();

    get_file_list(src_head, source_path);

    size_t destlen = strlen(destination_path);

    for (struct file_data* it = src_head->next; it; it = it->next) {
        printf("\n%s\n", it->path);
        time_t const* mtime = &it->mtime;
        printf("%s", ctime(mtime));

        size_t plen = strlen(it->path);
        char* path = malloc((destlen + plen + 1) * sizeof *path);

        strncpy(path, destination_path, destlen + 1);
        strncat(path, it->path, plen);
        printf("Path: %s\n", path);

        struct stat file_stat;

        bool copy_file = false;
        if (stat(path, &file_stat) == -1) {
            if (errno == ENOENT) {
                printf("Does not exist.\n");
                copy_file = true;
            } else {
                perror(NULL);
                return EXIT_FAILURE;
            }
        } else {
            printf("Dest file mtime: %s", ctime((time_t const*) &file_stat.st_mtime));
            double time_diff = difftime(it->mtime, file_stat.st_mtime);
            printf("Mtime diff: %g\n", time_diff);
            if (time_diff > 0.0) copy_file = true; 
        }

        if (copy_file) {
            printf("File must be copied.\n");
        }
    }

    free_list(src_head);

    return EXIT_SUCCESS;
}
