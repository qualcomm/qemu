/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#ifndef UTIL_H
#define UTIL_H 1

#include <stdint.h>

typedef struct {
    int is_open;
    int buf_allocated;
    void *buf;
    unsigned mode;
    long int buf_maxsize;
    long int crnt_filesize;
    long int pos;
} FILE_MEM;

FILE_MEM *fmemopen_mem(void *buf, int max, const char *mode);
int fclose_mem(FILE_MEM *fp);
int fgetc_mem(FILE_MEM *fp);
int fread_mem(void *ptr, int max, int nmemb, FILE_MEM *fp);
int fwrite_mem(void *ptr, int max, int nmemb, FILE_MEM *fp);
int frewind_mem(FILE_MEM *fp);
long fsize_mem(FILE_MEM *fp);
void pcycle_pause(uint64_t pcycle_wait);

#endif
