#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/time.h>

#define BUF_SIZE 4096


/**
 * Reports a change in a file or directory.
 *
 * This function prints the path of the file or directory along with a symbol
 * indicating the type of change that occurred.
 *
 * @param path The path of the file or directory that changed.
 * @param symbol The symbol indicating the type of change ('+' for added, '-' for deleted, 'o' for overwrite, 't' for timestamp change, 'p' for permission change).
 */
void report_change(char *path, char symbol) {
    printf("[%c] %s\n", symbol, path);
}


/**
 * Removes a directory and all its contents.
 *
 * This function recursively removes a directory and all its contents.
 *
 * @param path The path of the directory to remove.
 * @return 0 on success, -1 on error.
 */
int remove_directory(char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    struct stat entry_stat;
    char entry_path[BUF_SIZE];

    // Iterate over items in directory
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        sprintf(entry_path, "%s/%s", path, entry->d_name);

        if (lstat(entry_path, &entry_stat) == -1) {
            fprintf(stderr, "Error getting stat for %s: %s\n", entry_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode)) {
            // Recursively remove subdirectories
            if (remove_directory(entry_path) == -1) {
                fprintf(stderr, "Error removing directory %s\n", entry_path);
                continue;
            }
        } else {
            // Remove file
            if (remove(entry_path) == -1) {
                fprintf(stderr, "Error removing file %s: %s\n", entry_path, strerror(errno));
                continue;
            }
            report_change(entry_path, '-');
        }
    }

    // Close directory
    closedir(dir);

    // Remove the directory itself
    if (rmdir(path) == -1) {
        fprintf(stderr, "Error removing directory %s: %s\n", path, strerror(errno));
        return -1;
    }
    report_change(path, '-');

    return 0;
}


/**
 * Synchronizes the contents of two directories.
 *
 * This function recursively synchronizes the contents of two directories. It copies
 * files from the source directory to the destination directory if they do not exist
 * in the destination directory or if they are different. It also removes files from
 * the destination directory if they do not exist in the source directory. The function
 * also synchronizes the timestamps and permissions of the source and destination directories
 * and all the files and directories in them.
 *
 * @param src_path The path of the source directory.
 * @param dst_path The path of the destination directory.
 */
void sync_dirs(char *src_path, char *dst_path){
    DIR *src_dir, *dst_dir;

    src_dir = opendir(src_path);
    if (src_dir == NULL) {
        fprintf(stderr, "Error opening source directory (%s): %s\n", src_path, strerror(errno));
        return;
    }

    dst_dir = opendir(dst_path);
    if (dst_dir == NULL) {
        fprintf(stderr, "Error opening destination directory (%s): %s\n", dst_path, strerror(errno));
        return;
    }


    struct stat src_stat, dst_stat;
    struct dirent *src_entry, *dst_entry;
    char src_item_path[BUF_SIZE], dst_item_path[BUF_SIZE];

    // Iterate over items in source directory
    while ((src_entry = readdir(src_dir)) != NULL) {
        if (strcmp(src_entry->d_name, ".") == 0 || strcmp(src_entry->d_name, "..") == 0)
            continue;

        sprintf(src_item_path, "%s/%s", src_path, src_entry->d_name);
        sprintf(dst_item_path, "%s/%s", dst_path, src_entry->d_name);

        // reset errno to 0
        errno = 0;


        if (lstat(src_item_path, &src_stat) == -1) {
            fprintf(stderr, "Error getting stat for %s: %s\n", src_item_path, strerror(errno));
            continue;
        }
        if (lstat(dst_item_path, &dst_stat) == -1 && errno != ENOENT) {
            fprintf(stderr, "Error getting stat for %s: %s\n", dst_item_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(src_stat.st_mode)) {
            if (errno == ENOENT) {
                if (mkdir(dst_item_path, src_stat.st_mode) == -1) {
                    fprintf(stderr, "Error creating directory %s: %s\n", dst_item_path, strerror(errno));
                    continue;
                }
                report_change(dst_item_path, '+');
            }
            // if the dst_item_path exists but is not a directory, remove it and create a directory
            else if (!S_ISDIR(dst_stat.st_mode)){
                if (remove(dst_item_path) == -1) {
                    fprintf(stderr, "Error removing file %s: %s\n", dst_item_path, strerror(errno));
                    continue;
                }
                report_change(dst_item_path, '-');

                if (mkdir(dst_item_path, src_stat.st_mode) == -1) {
                    fprintf(stderr, "Error creating directory %s: %s\n", dst_item_path, strerror(errno));
                    continue;
                }
                report_change(dst_item_path, '+');
            }

            sync_dirs(src_item_path, dst_item_path);
        } else {
            if (errno == ENOENT) {
                // File does not exist in destination
                // Copy the file from source to destination
                int src_fd = open(src_item_path, O_RDONLY);
                if (src_fd == -1) {
                    fprintf(stderr, "Error opening file %s: %s\n", src_item_path, strerror(errno));
                    continue;
                }

                int dst_fd = open(dst_item_path, O_WRONLY | O_CREAT | O_EXCL, src_stat.st_mode);
                if (dst_fd == -1) {
                    fprintf(stderr, "Error creating file %s: %s\n", dst_item_path, strerror(errno));
                    close(src_fd);
                    continue;
                }

                ssize_t bytes_read, bytes_written;
                char buffer[BUF_SIZE];
                int success = 1;
                while ((bytes_read = read(src_fd, buffer, BUF_SIZE)) > 0) {
                    bytes_written = write(dst_fd, buffer, bytes_read);
                    if (bytes_written != bytes_read) {
                        fprintf(stderr, "Error writing to file %s\n", dst_item_path);
                        close(src_fd);
                        close(dst_fd);
                        remove(dst_item_path); // Remove partially copied file
                        success = 0;
                        break;
                    }
                }

                if (!success) {
                    continue;
                }

                report_change(dst_item_path, '+');

                close(src_fd);
                close(dst_fd);

            } else {
                int flag = 0;
                // dst_item_path exists but is not a file, remove it and create a file
                if (!S_ISREG(dst_stat.st_mode)){
                    if (remove_directory(dst_item_path) == -1) {
                        fprintf(stderr, "Error removing directory %s: %s\n", dst_item_path, strerror(errno));
                        continue;
                    }
                    flag = 1;
                }

                // File exists in both source and destination
                if (src_stat.st_size != dst_stat.st_size || src_stat.st_mtime != dst_stat.st_mtime || flag == 1) {

                    // If size, or time is different, overwrite destination
                    int src_fd = open(src_item_path, O_RDONLY);
                    if (src_fd == -1) {
                        fprintf(stderr, "Error opening file %s: %s\n", src_item_path, strerror(errno));
                        continue;
                    }

                    int dst_fd = open(dst_item_path, O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode);
                    if (dst_fd == -1) {
                        fprintf(stderr, "Error opening file %s: %s\n", dst_item_path, strerror(errno));
                        close(src_fd);
                        continue;
                    }
                    

                    ssize_t bytes_read, bytes_written;
                    char buffer[BUF_SIZE];
                    int success = 1;
                    while ((bytes_read = read(src_fd, buffer, BUF_SIZE)) > 0) {
                        bytes_written = write(dst_fd, buffer, bytes_read);
                        if (bytes_written != bytes_read) {
                            fprintf(stderr, "Error writing to file %s\n", dst_item_path);
                            close(src_fd);
                            close(dst_fd);
                            success = 0;
                            break;
                        }
                    }

                    if (!success) {
                        continue;
                    }

                    char modification = 'o';
                    if(flag == 1){
                        modification = '+';
                    }
                    report_change(dst_item_path, modification);

                    close(src_fd);
                    close(dst_fd);

                }
            }
        }
    }


    // Remove items from destination that do not exist in source
    rewinddir(dst_dir); // Reset directory stream pointer to the beginning

    // Iterate over items in destination directory
    while ((dst_entry = readdir(dst_dir)) != NULL) {
        if (strcmp(dst_entry->d_name, ".") == 0 || strcmp(dst_entry->d_name, "..") == 0)
            continue;

        sprintf(dst_item_path, "%s/%s", dst_path, dst_entry->d_name);
        sprintf(src_item_path, "%s/%s", src_path, dst_entry->d_name);

        // reset errno to 0
        errno = 0;

        if (lstat(dst_item_path, &dst_stat) == -1) {
            fprintf(stderr, "Error getting stat for %s: %s\n", dst_item_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(dst_stat.st_mode)) {
            // If directory in destination doesn't exist in source, recursively remove it
            if (lstat(src_item_path, &src_stat) == -1 && errno == ENOENT) {
                if (remove_directory(dst_item_path) == -1) {
                    fprintf(stderr, "Error removing directory %s: %s\n", dst_item_path, strerror(errno));
                }
            }
        } else {
            // If file in destination doesn't exist in source, remove it
            if (lstat(src_item_path, &src_stat) == -1 && errno == ENOENT) {
                if (remove(dst_item_path) == -1) {
                    fprintf(stderr, "Error removing file %s: %s\n", dst_item_path, strerror(errno));
                } else {
                    report_change(dst_item_path, '-');
                }
            }
        }
    }


    // Close directories
    closedir(src_dir);
    closedir(dst_dir);
}


/**
* Synchronizes the timestamps and permissions of two directories.
*
* This function recursively synchronizes the timestamps and permissions of two directories.
* It makes the timestamps and permissions of the destination directory the same as the source directory.
*
* @param src_path The path of the source directory.
* @param dst_path The path of the destination directory.
*/
void sync_time_permissions(char *src_path, char *dst_path){
    DIR *src_dir, *dst_dir;

    src_dir = opendir(src_path);
    if (src_dir == NULL) {
        fprintf(stderr, "Error opening source directory (%s): %s\n", src_path, strerror(errno));
        return;
    }

    dst_dir = opendir(dst_path);
    if (dst_dir == NULL) {
        fprintf(stderr, "Error opening destination directory (%s): %s\n", dst_path, strerror(errno));
        return;
    }

    struct stat src_stat, dst_stat;
    struct dirent *src_entry;
    char src_item_path[BUF_SIZE], dst_item_path[BUF_SIZE];

    // now both the source and destination directories have the same tree structure
    // so we can synchronize the timestamps and permissions of the source and destination directories
    // Iterate over items in source directory
    while ((src_entry = readdir(src_dir)) != NULL) {
        if (strcmp(src_entry->d_name, ".") == 0 || strcmp(src_entry->d_name, "..") == 0)
            continue;

        sprintf(src_item_path, "%s/%s", src_path, src_entry->d_name);
        sprintf(dst_item_path, "%s/%s", dst_path, src_entry->d_name);

        // reset errno to 0
        errno = 0;

        if (lstat(src_item_path, &src_stat) == -1) {
            fprintf(stderr, "Error getting stat for %s: %s\n", src_item_path, strerror(errno));
            continue;
        }
        if (lstat(dst_item_path, &dst_stat) == -1) {
            fprintf(stderr, "Error getting stat for %s: %s\n", dst_item_path, strerror(errno));
            continue;
        }

        // make the timestamp of the destination directory the same as the source directory
        if (S_ISDIR(src_stat.st_mode)) {
            sync_time_permissions(src_item_path, dst_item_path);
        }
        else{
            
            // open the source file
            int src_fd = open(src_item_path, O_RDONLY);
            if (src_fd == -1) {
                fprintf(stderr, "Error opening file %s: %s\n", src_item_path, strerror(errno));
                continue;
            }
            // open the destination file
            int dst_fd = open(dst_item_path, O_RDONLY);
            if (dst_fd == -1) {
                fprintf(stderr, "Error creating file %s: %s\n", dst_item_path, strerror(errno));
                close(src_fd);
                continue;
            }

            // make the timestamp of the destination file the same as the source file
            int time_changed = 0;
            if (src_stat.st_mtime != dst_stat.st_mtime) {
                time_changed = 1;
            }
            struct timeval times[2];
            times[0].tv_sec = src_stat.st_atime;
            times[0].tv_usec = 0;
            times[1].tv_sec = src_stat.st_mtime;
            times[1].tv_usec = 0;
            if (futimes(dst_fd, times) == -1) {
                fprintf(stderr, "Error updating timestamp for file %s: %s\n", dst_item_path, strerror(errno));
                continue;
            }
            if(time_changed){
                report_change(dst_item_path, 't');
            }

            // make the permissions of the destination file the same as the source file
            if (src_stat.st_mode != dst_stat.st_mode) {
                if (fchmod(dst_fd, src_stat.st_mode) == -1) {
                    fprintf(stderr, "Error updating permissions for file %s: %s\n", dst_item_path, strerror(errno));
                    continue;
                }
                report_change(dst_item_path, 'p');
            }

            // close the source and destination files
            close(src_fd);
            close(dst_fd);
        }
    }

    // if the source and destination directories have different timestamps, update the timestamp of the destination directory
    if (lstat(src_path, &src_stat) == -1) {
        fprintf(stderr, "Error getting stat for %s: %s\n", src_path, strerror(errno));
        return;
    }
    if (lstat(dst_path, &dst_stat) == -1) {
        fprintf(stderr, "Error getting stat for %s: %s\n", dst_path, strerror(errno));
        return;
    }
    int time_changed = 0;
    if (src_stat.st_mtime != dst_stat.st_mtime) {
        time_changed = 1;
    }
    struct timeval times[2];
    times[0].tv_sec = src_stat.st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = src_stat.st_mtime;
    times[1].tv_usec = 0;
    if (futimes(dirfd(dst_dir), times) == -1) {
        fprintf(stderr, "Error updating timestamp for file %s: %s\n", dst_path, strerror(errno));
        return;
    }
    if(time_changed){
        report_change(dst_path, 't');
    }

    // if the source and destination directories have different permissions, update the permissions of the destination directory
    if (src_stat.st_mode != dst_stat.st_mode) {
        if (fchmod(dirfd(dst_dir), src_stat.st_mode) == -1) {
            fprintf(stderr, "Error updating permissions for file %s: %s\n", dst_path, strerror(errno));
            return;
        }
        report_change(dst_path, 'p');
    }

    // Close directories
    closedir(src_dir);
    closedir(dst_dir);
    
}


/**
* Synchronizes the contents of two directories.
*
* This function calls the sync_dirs and sync_time_permissions functions to synchronize the contents
* of two directories. It copies files from the source directory to the destination directory if they do not exist
* in the destination directory or if they are different. It also removes files from the destination directory if they do not exist
* in the source directory. The function also synchronizes the timestamps and permissions of the source and destination directories
* and all the files and directories in them.
*
* @param src_path The path of the source directory.
* @param dst_path The path of the destination directory.
*/
void synchronize(char *src_path, char *dst_path) {
    sync_dirs(src_path, dst_path);
    sync_time_permissions(src_path, dst_path);
}



int main(int argc, char *argv[]) {
    // Check if the correct number of command line arguments are provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get the source and destination directory paths from command line arguments
    char *src_path = argv[1];
    char *dst_path = argv[2];

    // Call the synchronize function to synchronize the directories
    synchronize(src_path, dst_path);

    return 0;
}
