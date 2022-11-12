#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define VERSION "0.9 (2022-11-10)"

struct file_data {
    char* relative_path;
    char* full_path;
    time_t mtime;
    off_t size;
    mode_t mode;
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
Frees a single list node.
The node and its contained data are freed.
*/
void free_list_node(struct file_data* node) {
        free(node->full_path);
        free(node->relative_path);
        free(node);
}

/*
Frees the whole list identified by its 'head' node.
The head sentinel node is also freed.
*/
void free_list(struct file_data* head) {
    struct file_data* current = head->next;
    while (current) {
        struct file_data* next = current->next;
        free_list_node(current);
        current = next;
    }
    free_list_node(head); // free the sentinel
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
        free_list_node(obsolete_node);
    }
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
            if (current->relative_path == NULL) {
                sprintf(error_text, "Could not allocate memory for relative path");
                goto error_handling;
            }
            strncpy(current->relative_path, relative_path, relative_pathlen);

            current->mtime = file->fts_statp->st_mtime;
            current->size = file->fts_statp->st_size;
            current->mode = file->fts_statp->st_mode;
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

/*
Copies file 'from_path' to 'to_path'.
If 'to_path' already exists it will be truncated and overwritten.
If it does not exist a new file will be created. The new file's
mode is the 'mode' from 'from_path' without the setuid/setgid bits.
Remember that mode still gets modified by the umask(2) value.
See open(2).

This function expects that 'to_path' has already been tested and
whether it exists or not will be passed as a boolean value in
'to_exists'.
Error messages go to stderr and the function returns to the caller.
*/
void copy_file(
    char const*const from_path, 
    char const*const to_path, 
    off_t size,
    mode_t mode,
    bool to_exists
) {
    if (size == 0) {
        fprintf(stderr, "File size is 0: %s\n", from_path);
        return;
    }

    if (size > 8 * 1048576) {
        fprintf(
            stderr,
            "File size exceeds 8 MB: %s\n"
            "Copying large files is not implemented.\n",
            from_path
        );
        return;
    }

    int from_fd = open(from_path, O_RDONLY);
    if (from_fd == -1) {
        char msg[15 + strlen(from_path) + 1];
        sprintf(msg, "Could not open %s", from_path);
        perror(msg);
        return;
    }

    int to_fd;
    if (to_exists)
        to_fd = open(to_path, O_WRONLY | O_TRUNC);
    else
        to_fd = open(
            to_path,
            O_WRONLY | O_TRUNC | O_CREAT,
            mode & ~(S_ISUID | S_ISGID)
        );

    if (to_fd == -1) {
        char msg[15 + strlen(to_path) + 1];
        sprintf(msg, "Could not open %s", to_path);
        perror(msg);
        close(from_fd);
        return;
    }

    char* data = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, from_fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        goto error;
    }

    madvise(data, size, MADV_SEQUENTIAL);
    if (write(to_fd, data, size) != size) {
        perror("Could not write file");
        goto error;
    }

    if (munmap(data, size) == -1)
        perror("munmap failed");

error:
    close(to_fd);
    close(from_fd);
    return;
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

    for (struct file_data* src = src_head->next; src; src = src->next) {
        printf("\nFull path:     %s\n", src->full_path);
        printf("Relative path: %s\n", src->relative_path);
        printf("File size: %lld\n", src->size);

        bool copy = false;
        struct file_data* previous = search_list(dest_head, src->relative_path);
        struct file_data* destination_file = NULL;
        if (previous) {
            destination_file = previous->next;

            printf("Found at destination: %s\n", destination_file->full_path);

            time_t const* mtime = &src->mtime;
            printf("Source mtime:      %s", ctime(mtime));
            mtime = &destination_file->mtime;
            printf("Destination mtime: %s", ctime(mtime));

            double time_diff = difftime(src->mtime, destination_file->mtime);
            printf("Mtime diff: %g\n", time_diff);
            if (time_diff > 0.0)
                copy = true;
            else
                list_remove(previous);
        } else {
            printf("Not found at destination.\n");
            copy = true;
        }

        if (copy) {
            printf("File must be copied.\n");
            if (destination_file) {
                copy_file(
                    src->full_path, 
                    destination_file->full_path, 
                    src->size,
                    src->mode,
                    true
                );
                list_remove(previous);
            } else {
                size_t destlen = strlen(destination_path);
                size_t rplen = strlen(src->relative_path);
                char* dest_full_path = malloc((destlen + rplen + 1) * sizeof *dest_full_path);
                if (dest_full_path) {
                    strncpy(dest_full_path, destination_path, destlen + 1);
                    strncat(dest_full_path, src->relative_path, rplen);
                    copy_file(
                        src->full_path,
                        dest_full_path,
                        src->size,
                        src->mode,
                        false
                    );
                    free(dest_full_path);
                } else {
                    fprintf(
                        stderr, 
                        "Could not allocate memory for destination path.\n"
                        "File won't be copied: %s\n",
                        src->relative_path
                    );
                }
            }
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
