/*
 *  Copyright(c) 2019-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef _VTCM_H
#define _VTCM_H

#include "../hex_arch_types.h"
#include <stdint.h>
#include "../max.h"
#include "macros_auto.h"
#include "../arch.h"

typedef enum {
  POISON,
  SYNCED,
  SCAGA_LIST_TYPE_COUNT
} scaga_list_type_e;

typedef struct bytes_list_t bytes_list_t;
struct bytes_list_t {
  paddr_t paddr;
  size1u_t rw; // 0=sync, 1=R, 2=W, 3=RW
  int ct;
  bytes_list_t *next;
  bytes_list_t *prev;
};

typedef struct {
  	bytes_list_t *bytes[2];
  	bytes_list_t *first_byte[2];
  	bytes_list_t *last_byte[2];
    size4u_t byte_count[2]; 
  	scaga_callback_info_t scaga_info;
} vtcm_state_t;

void vtcm_init_state(vtcm_state_t *vtcm_state);
void vtcm_clear_state(vtcm_state_t *vtcm_state);
bytes_list_t *find_byte(vtcm_state_t *vtcm_state, paddr_t paddr, scaga_list_type_e type);
void enlist_byte(thread_t *thread, paddr_t paddr, scaga_list_type_e type, size1u_t rw);
void delist_byte(thread_t *thread, bytes_list_t *entry, scaga_list_type_e type);
void depoison_bytes_for_this_sync(thread_t *thread, paddr_t sync_paddr);
int check_load_acquire(thread_t *thread, paddr_t paddr);
void update_scaga_callback_info(processor_t *proc, scaga_callback_info_t *scaga, int tnum, vaddr_t pc, paddr_t pa, sg_event_type_e event);
#endif
