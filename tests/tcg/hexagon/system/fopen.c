/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
    size_t rc;
    char fileName[100];
    static char contents[6];
    FILE *fp;
    if (argc < 2) {
        printf("Usage: fopen <filename(s)>\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        sprintf(fileName, "%s", argv[i]);
        fp = fopen(fileName, "r+");
        if (!fp) {
            printf("FAIL: '%s': file not found", fileName);
            return 1;
        }
        errno = 0;
        rc = fread(contents, strlen("valid"), 1, fp);
        if (rc != 1) {
            printf("FAIL: file length mismatch!\n");
            fclose(fp);
            return 1;
        }
        if (strncmp(contents, "valid", strlen("valid"))) {
            printf("FAIL: file contents mismatch!\n");
            return 1;
        }
        printf("%s\n", contents);
        fclose(fp);
    }
    printf("PASS\n");
    return 0;
}
