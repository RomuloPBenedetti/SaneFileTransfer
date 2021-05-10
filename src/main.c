//
// Created by rbenedetti on 30/03/2021.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <btparse.h>
#include <stdbool.h>

int openFile (char *path, int flags, void *mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) {
        printf("\nUnable to open path: %s. err: %d\n", path, fd);
        exit(EXIT_FAILURE);
    }
    return fd;
}

struct stat fdStat (signed int fd) {
    struct stat fd_stat;
    fstat(fd, &fd_stat);
    return fd_stat;
}

void alg1(char *argv[], boolean sync) {
    /* Can be a bit slower with small files due to context switch overhead. */
    signed int source_file        = openFile(argv[2], O_RDONLY, NULL);
    size_t source_size_in_bytes = fdStat(source_file).st_size, mode = fdStat(source_file).st_mode;
    signed int dest_file = openFile(argv[3], O_WRONLY | O_CREAT | O_TRUNC, &mode);
    size_t transfered_in_bytes = 0;

    time_t start, end;
    double cpu_time = 0;

    size_t buffer_factor = 30;
    size_t buffer_size = buffer_factor * 1024 * 1024;

    while (transfered_in_bytes != source_size_in_bytes) {
        time(&start);

        if (sendfile(dest_file, source_file, NULL, buffer_size) != buffer_size) {
            printf("\nError writing to destiny.\n");
            exit(EXIT_FAILURE);
        }
        if (sync) {
            fdatasync(dest_file);
        }

        time(&end);
        cpu_time += difftime(end, start);

        transfered_in_bytes += buffer_size;
        size_t total_MB_writen = transfered_in_bytes / 1024 / 1024;
        size_t percent         = transfered_in_bytes * 100 / source_size_in_bytes;
        double speed           = total_MB_writen / cpu_time;

        if (speed < 500) {
            buffer_size = 1024 * speed / 4 * getpagesize() * 2;
            if ((transfered_in_bytes + buffer_size) > source_size_in_bytes) {
                buffer_size = source_size_in_bytes - transfered_in_bytes;
            }
        }

        printf("\r");
        printf("Witen: %luMB | %lu%% | %.2lfMBps | %.2lfsec",
               total_MB_writen, percent, speed, cpu_time);
        fflush(stdout);
    }
    posix_fadvise(source_file, 0,0,POSIX_FADV_DONTNEED);

    printf("\nFile copied successfully.\n");
    close(source_file);
    close(dest_file);
};

void alg2(char *argv[], boolean sync) {
    /* Can be a bit slower with big files due to dirty page scheduling overhead. */
    signed int source_file        = openFile(argv[2], O_RDONLY, NULL);
    size_t source_size_in_bytes = fdStat(source_file).st_size, mode = fdStat(source_file).st_mode;
    signed int dest_file = openFile(argv[3], O_WRONLY | O_CREAT | O_TRUNC, &mode);
    off_t offset = 0;

    time_t start, end;
    double cpu_time = 0;
    time(&start);
    printf("Transfearing.\n", cpu_time);

    size_t total_sent_in_bytes = sendfile(dest_file, source_file, &offset, source_size_in_bytes);
    while (total_sent_in_bytes < source_size_in_bytes) {
        size_t sent_in_bytes = source_size_in_bytes - total_sent_in_bytes;
        total_sent_in_bytes += sendfile(dest_file, source_file, &offset, sent_in_bytes);
    }
    posix_fadvise(source_file, 0,0,POSIX_FADV_DONTNEED);
    if (sync) {
        fdatasync(dest_file);
    }

    time(&end);
    cpu_time += difftime(end, start);

    printf("\nFile copied successfully in %.2lfsec.\n", cpu_time);
    close(source_file);
    close(dest_file);
};

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Wrong number of arguments;\n"
                        "Usage: --alg[1,2]_[async,sync] %s <from_path> <to_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "--alg1_async") == 0) {
        alg1(argv, false);
    } else if (strcmp(argv[1], "--alg1_sync") == 0) {
        alg1(argv, true);
    } else if (strcmp(argv[1], "--alg2_async") == 0) {
        alg2(argv, false);
    } else if (strcmp(argv[1], "--alg2_sync") == 0) {
        alg2(argv, true);
    } else {
        fprintf(stderr, "Wrong algorithm name;\n"
                        "Usage: --alg[1,2]_[async,sync] %s <from_path> <to_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}
