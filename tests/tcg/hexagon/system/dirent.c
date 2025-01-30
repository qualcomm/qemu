/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "strutils.h"

#define ERRNO_SENTINEL 1000 /* An invalid error number */

#define ERR(msg) do { \
        printf("fatal: %s: (%d) %s\n", msg, errno, strerror(errno)); \
        exit(1); \
    } while (0)

#define MAX 10

int main(int argc, char **argv)
{
    char *found_files[MAX];
    int n = 0;
    assert(argc == 2);
    DIR *dirp = opendir(argv[1]);
    if (!dirp) {
        ERR("couldn't open dir");
    }

    while (1) {
        errno = ERRNO_SENTINEL;
        struct dirent *dp = readdir(dirp);
        if (!dp) {
            /* FIXME: this check is currently disabled due
             * to a bug in hexagon-libc (see QTOOL-106440).
             */
            /*
            if (errno != ERRNO_SENTINEL) {
                ERR("error reading directory");
            }
            */
            break;
        }
        if (n < MAX) {
            found_files[n] = strdup(dp->d_name);
            assert(found_files[n]);
        } else {
            printf("fatal: cannot list more than %d files\n", MAX);
            return 1;
        }
        n++;
    }

    sort_str_arr(found_files, n);
    bool first = true;
    for (int i = 0; i < n; i++) {
        printf("%s%s", first ? "" : " ", found_files[i]);
        free(found_files[i]);
        first = false;
    }

    if (closedir(dirp)) {
        ERR("closedir error");
    }
    return 0;
}
