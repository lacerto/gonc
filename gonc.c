#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>

#define VERSION "0.3 (2022-11-06)"

struct file_data {
    char* relative_path;
    char* full_path;
    time_t mtime;
    struct file_data* next;
};

void print_version() {
    printf("This is gonc version " VERSION ".\n");
}

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

/*
Checks whether the path is a directory and returns
true if it is.
Returns false if the path does not point to a directory,
the path does not exists or an error occurred trying to call
stat(2) on the path.
*/
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

/*
Removes a trailing directory separator (currently only
works for the forward slash) from the end of the path
string.
Path is not modified if it is an empty string or its 
last character is not a forward slash.
*/
void remove_trailing_slash(char* const path) {
    size_t length = strlen(path);
    if (length == 0) return;

    char* p = path + (length -1);
    if (*p == '/') *p = '\0';
}

/*
Returns a sentinel node that serves as the head of
the singly linked list.
This is a dummy node containing no data.
Terminates the program if memory cannot be allocated.
*/
struct file_data* get_sentinel_node() {
    struct file_data* sentinel = malloc(sizeof *sentinel);
    if (sentinel == NULL) {
        perror("Failed to allocate memory for list head");
        exit(EXIT_FAILURE);
    } else {
        sentinel->full_path = NULL;
        sentinel->relative_path = NULL;
        sentinel->mtime = 0;
        sentinel->next = NULL;
        return sentinel;
    }
}

/*
Inserts a node 'new' in the singly linked list after
node 'current'.
If 'current' is the last node in the list then this
function simply appends 'new' at the end of the list.
*/
void list_insert(struct file_data* restrict current, struct file_data* restrict new) {
    new->next = current->next;
    current->next = new;
}

/*
Removes the node from thee singly linked list that
comes right after the 'previous' node.
The removed node and its contained data are freed.
*/
void list_remove(struct file_data* const previous) {
    struct file_data* obsolete_node = previous->next;
    if (obsolete_node) {
        previous->next = obsolete_node->next;
        free(obsolete_node->full_path);
        free(obsolete_node->relative_path);
        free(obsolete_node);
    }
}

/*
Frees the whole list identified by its 'head' node.
The head sentinel node is also freed.
*/
void free_list(struct file_data* head) {
    struct file_data* current = head->next;
    while (current) {
        struct file_data* next = current->next;
        free(current->full_path);
        free(current->relative_path);
        free(current);
        current = next;
    }
    free(head); // free the sentinel
}

/*
Searches in the list identified by 'head' for a node, whose
relative_path matches the 'path' parameter.
If found returns the *previous* node in the list.
Returns NULL otherwise.
*/
struct file_data* search_list(struct file_data* const head, char* const path) {
    struct file_data* previous = head;
    struct file_data* current = head->next;

    while (current) {
        if (strcmp(current->relative_path, path) == 0) {
            return previous;
        }
        previous = current;
        current = current->next;
    }
    return NULL;
}

/*
Traverses the file hierarchy starting at directory 'dirpath'.
Stores every regular file in the singly linked list identified
by 'head'.
Directories are not stored in the list, symbolic links are 
followed.
Terminates the program if memory cannot be allocated.
See fts(3).
*/
void get_file_list(struct file_data* head, char* const dirpath) {
    FTS* file_hierarchy;
    FTSENT* file;
    char error_text[50] = "";

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
                sprintf(error_text, "Could not allocate memory for file data");
                goto error_handling;
            }

            current->full_path = malloc((file->fts_pathlen + 1) * sizeof *current->full_path);
            if (current->full_path == NULL) {
                sprintf(error_text, "Could not allocate memory for full path");
                goto error_handling;
            }
            strncpy(current->full_path, file->fts_path, file->fts_pathlen + 1);

            char const*const relative_path = file->fts_path + strlen(dirpath);
            size_t relative_pathlen = strlen(relative_path) + 1; // including terminating '\0'

            current->relative_path = malloc(relative_pathlen * sizeof *current->relative_path);
            if (current->full_path == NULL) {
                sprintf(error_text, "Could not allocate memory for relative path");
                goto error_handling;
            }
            strncpy(current->relative_path, relative_path, relative_pathlen);

            current->mtime = file->fts_statp->st_mtime;
            current->next = NULL;

            list_insert(previous, current);
            previous = current;
        }
    }
    return;

error_handling:
    perror(error_text);
    free_list(head);
    exit(EXIT_FAILURE);
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

    struct file_data* dest_head = get_sentinel_node();
    get_file_list(dest_head, destination_path);

    size_t destlen = strlen(destination_path);

    for (struct file_data* it = src_head->next; it; it = it->next) {
        printf("\nFull path:     %s\n", it->full_path);
        printf("Relative path: %s\n", it->relative_path);

        bool copy_file = false;
        struct file_data* previous = search_list(dest_head, it->relative_path);
        if (previous) {
            struct file_data* destination_file = previous->next;

            printf("Found at destination: %s\n", destination_file->full_path);

            time_t const* mtime = &it->mtime;
            printf("Source mtime:      %s", ctime(mtime));
            mtime = &destination_file->mtime;
            printf("Destination mtime: %s", ctime(mtime));

            double time_diff = difftime(it->mtime, destination_file->mtime);
            printf("Mtime diff: %g\n", time_diff);
            if (time_diff > 0.0) copy_file = true; 
            list_remove(previous);
        } else {
            printf("Not found at destination.\n");
            copy_file = true;
        }

        if (copy_file) {
            printf("File must be copied:\n");
        } else {
            printf("File won't be copied.\n");
        }
    }

    printf("\nFiles not present in source:\n");
    struct file_data* file = dest_head->next;
    while (file) {
        printf("\tFull path:     %s\n", file->full_path);
        printf("\tRelative path: %s\n", file->relative_path);
        file = file->next;
    }

    free_list(src_head);
    free_list(dest_head);

    return EXIT_SUCCESS;
}
