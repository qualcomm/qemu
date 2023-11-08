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

#if 0
#include "arch/thread.h"
#else
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#define pfatal(...)
#define CACHE_ASSERT(...)
#define ARCH_GET_PCYCLES(x) 0
#endif
#include "dma_adapter.h"
#include "desc_tracker.h"



/* private */
/* Get unique id for transaction */

static inline desc_tracker_t * get_desc_tracker(processor_t *proc, int dmanum) {
	dma_adapter_engine_info_t * engine_info = (dma_adapter_engine_info_t *) proc->dma_engine_info[dmanum];
    return &engine_info->desc_tracker;
}


static inline desc_id_t desc_tracker_getid(processor_t *proc, int dmanum)
{
    desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
    return ++tracker->next_id;
}



#if defined(DEBUG) && (DEBUG == 0 && DESC_DBG == 0)

#define desc_tracker_calloc(tracker, size) object_pool_calloc(&tracker->pool, size)
#define desc_tracker_free_pool(tracker, ptr) object_pool_free(&tracker->pool, ptr)

#else

#define desc_tracker_calloc(tracker, size) calloc(1, size)
#define desc_tracker_free_pool(tracker, ptr) free(ptr)

#endif


static const desc_tracker_entry_t * desc_tracker_from_node(const dll_node_t *node)
{
	/* do byte math, node dll_node_t-sized math */
	return (desc_tracker_entry_t *)((char *)node - offsetof(desc_tracker_entry_t, node));
}

int desc_tracker_release(processor_t *proc, int dmanum, desc_tracker_entry_t * entry)
{
	//printf("dma%d release desc=%lli\n",dmanum, entry->id);
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	if (sll_contains(&tracker->valid_list.forward, &entry->node.forward) == 0) {
      	pfatal(proc, "%llu: Attempted to free invalid entry %d\n", proc->monotonic_pcycles, (unsigned int) entry->id);
    }
	dll_node_t * const node = &entry->node;
	dll_remove(node);

	// Move to free list
	dll_insert_after(&tracker->pending_free_list, node);
	tracker->valid_list_num--;
	CACHE_ASSERT(proc, tracker->valid_list_num >= 0);
	return 1;
}

static desc_tracker_entry_t * desc_tracker_getnext(processor_t *proc, int dmanum)
{
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	desc_tracker_entry_t * entry = (desc_tracker_entry_t *) desc_tracker_calloc(tracker, sizeof(*entry));
	assert(entry && "calloc failure");
	dll_insert_after(&tracker->valid_list, &entry->node);
	tracker->valid_list_num++;
	return entry;
}

void desc_tracker_free_list(processor_t *proc, desc_tracker_t *tracker, dll_node_t *list);
void desc_tracker_free_list(processor_t *proc, desc_tracker_t *tracker, dll_node_t *list)
{
	dll_node_t *walk = list->next;
	while (walk != NULL) {
		dll_node_t * const to_free = walk;
		walk = walk->next;
		to_free->next = NULL;
		to_free->prev = NULL;
		desc_tracker_free_pool(tracker, to_free);
	}
	list->next = NULL;
}

int desc_tracker_cycle(processor_t *proc, int dmanum)
{
	// Free the free list
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	dll_node_t *list = &tracker->pending_free_list;
	desc_tracker_free_list(proc, tracker, list);
	return 1;
}

void desc_tracker_init(processor_t * proc, int dmanum)
{
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	const dll_node_t empty = {};
	tracker->valid_list = empty;
	tracker->valid_list_num = 0;
	tracker->pending_free_list = empty;
	object_pool_init( &tracker->pool, (char *)&tracker->entry_storage, sizeof(tracker->entry_storage[0]), DESC_TABLESIZE);
}

int desc_unreleased_check ( processor_t *proc, int dmanum);
int
desc_unreleased_check ( processor_t *proc, int dmanum)
{
  	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	dll_node_t *walk = &tracker->valid_list;
    desc_tracker_entry_t *desc;
    int stuck = 0;
	while (walk != NULL) {
      	desc = (desc_tracker_entry_t *)((char *)walk - offsetof(desc_tracker_entry_t, node));
        if (desc) {
            if (desc->pcycle) {
                stuck = 1;
            }
        }
		walk = walk->next;
	}
    return stuck;
}

void desc_tracker_free(processor_t * proc, int dmanum)
{
	// Free the valid list
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	dll_node_t *list = &tracker->valid_list;
	desc_unreleased_check (proc, dmanum);
	desc_tracker_free_list(proc, tracker, list);
	tracker->valid_list_num = 0;
}


void desc_tracker_dump_one(const processor_t *proc, const desc_tracker_entry_t * entry, FILE *file);
void desc_tracker_dump_one(const processor_t *proc, const desc_tracker_entry_t * entry, FILE *file)
{
	if (entry == NULL) {
		fputs("null", file);
	} else{
		fprintf(file, "{ id:%lld, birth:%"PRIu64"", entry->id, entry->pcycle);
		//fprintf(file, ", pc_va:%X, paddr:%llX, pktid:%lld, ma_type:%d",
		//		desc->ma.pc_va, desc->ma.paddr, entry->uma.packet_id, desc->ma.type);
		//fprintf(file, ", uma_type: %d", desc->uma.access_type);
		fputs(" }", file);
	}
}

void desc_tracker_dump_all(processor_t *proc, int dmanum, FILE *file);
void desc_tracker_dump_all(processor_t *proc, int dmanum, FILE *file)
{
	desc_tracker_t * const tracker = get_desc_tracker(proc, dmanum);
	const dll_node_t *walk = tracker->valid_list.next;

	while (walk != NULL) {
		desc_tracker_dump_one(proc, desc_tracker_from_node(walk), file);
		fputs(",\n", file);

		walk = walk->next;
	}
}


/* public */
desc_tracker_entry_t * desc_tracker_acquire ( processor_t *proc, int dmanum, dma_decoded_descriptor_t * desc)
{
	desc_tracker_entry_t * entry = desc_tracker_getnext(proc, dmanum);
	entry->id = desc_tracker_getid(proc, dmanum); /* unique to entry */
	entry->pcycle = ARCH_GET_PCYCLES(proc); /* birth cycle */
    entry->desc = *desc;
	entry->dnum = dmanum;
	entry->dma = NULL;
	//printf("dma%d acquire desc=%lli\n",dmanum, entry->id);
	return entry;
}

