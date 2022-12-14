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

#define VERSION "1.4 (2022-12-09)"

//#define DEBUG

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
    printf("\tgonc [-d] [-n] source_dir destination_dir\n");
    printf("\tgonc -h\n");
    printf("\tgonc -v\n");
    printf("\nOptions:\n");
    printf("\t-d\tDelete files at destination that are not in source.\n");
    printf("\t-n\tDry run. No files will be copied or deleted.\n");
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
            // skip dotfiles
            if (file->fts_namelen > 1) {
                if (file->fts_name[0] == '.') continue;
            }

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
Error messages go to stderr and the function returns false to the
caller. Return value is true if the copy had been succeessful.
*/
bool copy_file(
    char const*const from_path, 
    char const*const to_path, 
    off_t size,
    mode_t mode,
    bool to_exists
) {
    if (size == 0) {
        fprintf(stderr, "\tFile size is 0: %s\n", from_path);
        return false;
    }

    int from_fd = open(from_path, O_RDONLY);
    if (from_fd == -1) {
        char msg[15 + strlen(from_path) + 1];
        sprintf(msg, "\tCould not open %s", from_path);
        perror(msg);
        return false;
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
        sprintf(msg, "\tCould not open %s", to_path);
        perror(msg);
        close(from_fd);
        return false;
    }

    // file is larger than 8 MB
    if (size > 8 * 1048576) {
        const size_t buf_size = 64 * 1024;
        char *buf;
        ssize_t bytes_read;
        ssize_t bytes_written;
        bool error_occurred = false;

        buf = malloc(buf_size);
        if (buf == NULL) {
            perror("Could not allocate memory for buffer (copy_file)");
            goto error;
        }

        while ((bytes_read = read(from_fd, buf, buf_size)) > 0) {
            bytes_written = write(to_fd, buf, bytes_read);
            if (bytes_read != bytes_written || bytes_written == -1) {
                if (errno) {
                    perror(NULL);
                }
                fprintf(stderr, "\tCopy error, number of read & written bytes do not match.\n");
                goto error;
            }
        }

        if (bytes_read == -1) {
            perror(NULL);
            goto error;
        }

        close(to_fd);
        close(from_fd);
        return true;
    }

    char* data = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, from_fd, 0);
    if (data == MAP_FAILED) {
        perror("\tmmap failed.");
        goto error;
    }

    madvise(data, size, MADV_SEQUENTIAL);
    if (write(to_fd, data, size) != size) {
        perror("\tCould not write destination file.");
        goto error;
    }

    if (munmap(data, size) == -1) {
        perror("\tmunmap failed.");
        goto error;
    }

    close(to_fd);
    close(from_fd);
    return true;

error:
    close(to_fd);
    close(from_fd);
    return false;
}

/*
Creates all the intermediate directories in the path if they
do not exist.
The full path consists of 'base' and 'relative'. 'base' must
exist and be a directory, this function only cares about the
directories in the 'relative' path.
Returns true if all directories have been created successfully,
false in the case of any errors.
Terminates the whole program if memory cannot be allocated.
*/
bool create_path(char const*const base, char const*const relative) {
    struct stat dir_stat;

    size_t base_len = strlen(base);
    size_t relative_len = strlen(relative);

    char* full_path = malloc((base_len + relative_len + 1) * sizeof *full_path);
    if (full_path == NULL) {
        perror("Could not allocate memory for path (create_path)");
        exit(EXIT_FAILURE);
    }

    strncpy(full_path, base, base_len + 1);
    strncat(full_path, relative, relative_len);

    // position p at the start of the relative path
    char* p = full_path + base_len;

    while(true) {
        p += strspn(p, "/");
		p += strcspn(p, "/");
        if (*p == '\0') break;

		*p = '\0';

        if (stat(full_path, &dir_stat) == -1) {
            if (errno == ENOENT) {
                if (mkdir(full_path, 0777) == -1) {
                    perror("Could not create directory");
                    free(full_path);
                    return false;
                }
            } else {
                perror("Could not check directory, stat failed");
                free(full_path);
                return false;
            }
        } else {
            if (!S_ISDIR(dir_stat.st_mode)) {
                fprintf(stderr, "Not a directory: %s\n", full_path);
                free(full_path);
                return false;
            }
        }

        *p = '/';
    }
    free(full_path);
    return true;
}

int main(int argc, char* argv[argc+1]) {
    extern int optind;
    char ch;
    bool delete_flag = false;
    bool dry_run_flag = false;

    while ((ch = getopt(argc, argv, "hvdn")) != -1) {
        switch (ch) {
            case 'v':
                print_version();
                return EXIT_SUCCESS;
            case 'h':
                print_usage();
                return EXIT_SUCCESS;
            case 'd':
                delete_flag = true;
                break;
            case 'n':
                dry_run_flag = true;
                break;
            case '?':
            default:
                print_usage();
                return EXIT_FAILURE;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    print_version();

    if (dry_run_flag) printf("\n** DRY RUN **\n");

    char* const source_path = argv[0];
    char* const destination_path = argv[1];

    remove_trailing_slash(source_path);
    remove_trailing_slash(destination_path);

    if (!isdir(source_path)) return EXIT_FAILURE;
    if (!isdir(destination_path)) return EXIT_FAILURE;

    struct file_data* src_head = get_sentinel_node();
    get_file_list(src_head, source_path);

    struct file_data* dest_head = get_sentinel_node();
    get_file_list(dest_head, destination_path);

    for (struct file_data* src = src_head->next; src; src = src->next) {
#ifdef DEBUG
        printf("\nFull path:     %s\n", src->full_path);
        printf("Relative path:   %s:\n", src->relative_path);
        printf("File size: %lld\n", (long long)src->size);
#endif
        bool copy = false;
        struct file_data* previous = search_list(dest_head, src->relative_path);
        struct file_data* destination_file = NULL;
        if (previous) {
            destination_file = previous->next;

#ifdef DEBUG
            printf("Found at destination: %s\n", destination_file->full_path);

            time_t const* mtime = &src->mtime;
            printf("Source mtime:      %s", ctime(mtime));
            mtime = &destination_file->mtime;
            printf("Destination mtime: %s", ctime(mtime));
#endif

            double time_diff = difftime(src->mtime, destination_file->mtime);

#ifdef DEBUG
            printf("Mtime diff: %g\n", time_diff);
#endif

            if (time_diff > 0.0) {
                copy = true;
                printf("\n%s:\n", src->relative_path);
                printf("\tFile is outdated.\n");
            } else
                list_remove(previous);
        } else {
            copy = true;
            printf("\n%s:\n", src->relative_path);
            printf("\tFile does not exist at destination.\n");
        }

        if (copy && !dry_run_flag) {
            if (destination_file) {
                bool copy_success = copy_file(
                    src->full_path, 
                    destination_file->full_path, 
                    src->size,
                    src->mode,
                    true
                );
                list_remove(previous);
                if (copy_success)
                    printf("\tUpdated.\n");
            } else {
                size_t destlen = strlen(destination_path);
                size_t rplen = strlen(src->relative_path);
                char* dest_full_path = malloc((destlen + rplen + 1) * sizeof *dest_full_path);
                if (dest_full_path) {
                    strncpy(dest_full_path, destination_path, destlen + 1);
                    strncat(dest_full_path, src->relative_path, rplen);

                    if (create_path(destination_path, src->relative_path)) {
                        bool copy_success = copy_file(
                            src->full_path,
                            dest_full_path,
                            src->size,
                            src->mode,
                            false
                        );
                        if (copy_success)
                            printf("\tCreated.\n");
                    }
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
        }
    }

    if (delete_flag) {
        printf("\nDeleting files not present in source:\n");
        struct file_data* file = dest_head->next;
        while (file) {
            printf("\t%s\n", file->full_path);
            if (!dry_run_flag) {
                if (unlink(file->full_path) != 0) {
                    perror("\t\tCould not delete file");
                }
            }
            file = file->next;
        }
    }

    free_list(src_head);
    free_list(dest_head);

    return EXIT_SUCCESS;
}
