/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEX_COPROC_H
#define HEX_COPROC_H
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
    int32_t opcode;
    hwaddr vtcm_base;
    uint32_t vtcm_size;
    uint8_t minver;
    uint8_t unit;
    uint16_t spare;
    uint32_t reg_usr;
    uint32_t subsystem_id;
    int32_t page_size;
    int32_t arg1;
    int32_t arg2;
} CoprocArgs;

void coproc(const CoprocArgs *args);

#endif
