/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
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
