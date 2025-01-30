/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>


static uint8_t buffer[1024];

static const char *test_case_filename = "test_case.dat";
static const char *missing_filename = "256ece7c-1a77-4031-b0dd-c3cbd98ba971";

static void cleanup()
{
    unlink(test_case_filename);
}

static void test_file_io()
{
    int ret, fd;
    DIR *dir;
    bool found_case = false;
    struct dirent *entry = NULL;
    static uint8_t read_buffer[1024];
    int read_fd, bytes_read;

    memset(buffer, 0xa5, sizeof(buffer));

    fd = open(test_case_filename, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    assert(fd != -1);

    ret = write(fd, buffer, sizeof(buffer));
    assert(ret != -1);

    ret = close(fd);
    assert(ret != -1);

    atexit(cleanup);

    dir = opendir(".");
    if (!dir) {
        printf("FAIL\n");
        perror("directory open failed");
        exit(3);
    }

    while ((entry = readdir(dir))) {
        found_case = found_case || (strcmp(entry->d_name, test_case_filename) == 0);
    }
    assert(found_case);
    closedir(dir);

    memset(read_buffer, 0x99, sizeof(read_buffer));
    read_fd = open(test_case_filename, O_RDONLY);
    bytes_read = read(read_fd, read_buffer, sizeof(read_buffer));
    assert(bytes_read == sizeof(read_buffer));

    close(read_fd);

    assert(memcmp(read_buffer, buffer, sizeof(buffer)) == 0);
}

static void test_missing_file()
{
    int fd = open(missing_filename, O_RDONLY);
    assert(errno == ENOENT);
    assert(fd == -1);
}

int main()
{
    test_file_io();
    test_missing_file();
    printf("PASS\n");
}
