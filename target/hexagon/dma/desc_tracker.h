/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef _DESC_TRACKER_H
#define _DESC_TRACKER_H

#if 0
#include "arch/thread.h"
#endif
#include "uarch/lll.h"
#include "uarch/object_pool.h"
#include "dma_descriptor.h"

#define DESC_TABLESIZE  512

typedef unsigned long long desc_id_t;

struct DMA;

typedef void (*callback_desc_t)(void * desc_entry);


/// A single microarchitectural memory transaction.
typedef struct desc_tracker_entry_t {
	dll_node_t node;

	/* book keeping */
	desc_id_t id; /* xact id */
	size8u_t pcycle;  /* time acquired */

	void * dma;
	int dnum; /* DMA number */

	dma_decoded_descriptor_t uarch_desc;	///< initial descriptor - before executed by ISS and used by timing uarch model

	dma_decoded_descriptor_t desc;		///< desc - after executed by ISS. May be done or stopped at exception point.
	callback_desc_t callback;

} desc_tracker_entry_t;

/// A resource manager that tracks outstanding (incomplete) memory
/// transactions and handles allocation and deallocation of the internal
/// structures used for tracking.
typedef struct desc_tracker_t {
	dll_node_t valid_list; //< Incomplete allocated xacts
	dll_node_t pending_free_list; //< xacts that will be freed at the end of the current clock cycle

	int valid_list_num;
	desc_id_t next_id; //< The id that will be used for the next newly acquired xact.

	object_pool_t pool; //< The object pool used to acquire new xacts.
	desc_tracker_entry_t entry_storage[DESC_TABLESIZE]; //< The static storage used to hold xacts in the pool.
} desc_tracker_t;


desc_tracker_entry_t* desc_tracker_acquire(struct ProcessorState *proc, int dmanum, dma_decoded_descriptor_t * desc);
int desc_tracker_release(struct ProcessorState *proc, int dmanum, desc_tracker_entry_t *desc);
int desc_tracker_cycle(struct ProcessorState *proc, int dmanum);
void desc_tracker_init(struct ProcessorState *proc, int dmanum);
void desc_tracker_free(struct ProcessorState *proc, int dmanum);

#endif

