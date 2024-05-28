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

/**
 * @brief User-DMA timing model
 * http://qwiki.qualcomm.com/hexagon-architecture/Dma_model
 *
 */

#ifndef _UARCH_DMA_H_
#define _UARCH_DMA_H_

#include "uarch/queue.h"
#include "dma/desc_tracker.h"

// mini-TLB ===========================================================
typedef enum dma_mini_tlb_idx {	
	DMA_TLB_ENTRY_SRC,			///< Source read address
	DMA_TLB_ENTRY_DST,			///< Destination write address
	DMA_TLB_ENTRY_DLBC,			///< Meta data read address (DLBC)
	DMA_TLB_ENTRY_PFETCH,		///< Descriptor Pre-Fetcher
	DMA_TLB_ENTRY_RETRANS,		///< Descriptor re-translation
	DMA_TLB_ENTRY_MAX
} dma_mini_tlb_idx_t;

typedef enum dma_tlb_status {	
	DMA_TLB_STS_MTLB_HIT,			///< mini-TLB hit
	DMA_TLB_STS_MTLB_RELOAD,		///< mini-TLB miss, JTLB hit, triggering mini-TLB reload
	DMA_TLB_STS_JTLB_MISS,			///< mini-TLB miss, JTLB miss
} dma_tlb_status_t;

typedef struct dma_mini_tlb_entry_t {
	bool		valid;				///< Marked valid as soon as entry is loaded but entru shouldn't be used til reloadCountdown complete.
	uint32_t	reloadCountdown;	///< Countdown timer for miniTLB entry reload from jtlb
	uint32_t	jtlb_idx;			///< Index in jtlb. Used for tlbw based invalidates.
	uint64_t	rawEntry;			///< Raw 64b TLB entry
} dma_mini_tlb_entry_t;


typedef enum uarch_dma_enigne_state {
	UARCH_DMA_STATE_IDLE,			///< All descriptors in fifo are DEFS_IDLE
	UARCH_DMA_STATE_ACTIVE,			///< Prefetcher still active
	UARCH_DMA_STATE_END_OF_LIST,	///< Prefetcher hit end of list and has gone idle but some descriptors still in-progress
} uarch_dma_enigne_state_t;

typedef enum uarch_desc_state {
	UARCH_DESC_STATE_IDLE = 0,
	
	// only 1 entry can be these states in a given tick. We can't start the next desc until we've fetched the desc & get the next*.
	UARCH_DESC_STATE_START,				///< va & descTrackerEntryP populated via pop from inputQ. prefetch engine busy.
	UARCH_DESC_STATE_XLATE_STALL,		///< descriptor address xlation
	UARCH_DESC_STATE_FETCH_STALL,
	UARCH_DESC_STATE_FETCH_COMPLETE,
	UARCH_DESC_STATE_ORDER_STALL,		///< We have the descriptor but order=1 & waiting for previous descriptor to DONE
	UARCH_DESC_STATE_PREF_NEXT,			///< Queue up prefetch of next descriptor. prefetch engine freed.

	UARCH_DESC_STATE_READY,				///< Queue up for decomposition

	// only 1 entry can be this state in a given tick.
	UARCH_DESC_STATE_DECOMP,			///< decomposition (issuing read ops). decomp engine busy.
	
	UARCH_DESC_STATE_DECOMP_DONE,		///< decomposition complete, waiting for return data & writes to complete. decomp engine freed.
	UARCH_DESC_STATE_XFERS_DONE,		///< Data xfers are complete & elligible to mark DONE (but list must be marked Done in chained order)
	UARCH_DESC_STATE_DONE,				///< Signals the Descriptor update unit to update desc & pop
	UARCH_DESC_STATE_NUM
} uarch_desc_state_t;



// The packer used to contain actual data buffers. Since the timing model now doesn't do any actual mem r/w's
//		it just keeps track of buffered byte counts & outstanding transaction counts
typedef struct descPacker_t {
	uint32_t	data_bytes;				///< Bytes in buffer.
	uint32_t	data_bytes_pending;		///< for timing mode
	uint32_t	outstanding_reads;		///< For tracking when we've received all outstanding xactions
	uint32_t	outstanding_writes;		///< For tracking when we've received all outstanding xactions
} descPacker_t;

typedef struct dma_state dma_t;		// Forward declaration needed here

typedef struct uarch_dma_desc_queue_entry {
	va_t					va;
	pa_t					pa;
	uarch_desc_state_t		state;
	bool					fetchIssued;
	bool 					started_decomp;
	uint32_t				countdown;		///< State transition timer used for fixed stall latencies
	HEXAGON_DmaDescriptor_t	desc;
	descPacker_t 			packer;				///< per-descriptor read data buffer
	desc_tracker_entry_t * 	arch_tracker_entry;
	dma_t    				*dma;				///< pointer back to dma engine instance
} uarch_dma_desc_queue_entry_t;


typedef struct uarch_dma_fetch_desc_queue {
	queue_t					queue;
	uarch_dma_desc_queue_entry_t * entries[DESC_TABLESIZE];
} uarch_dma_fetch_desc_queue_t;

typedef struct uarch_dma_desc_queue {
	queue_t					queue;
	uarch_dma_desc_queue_entry_t  entries[DESC_TABLESIZE];
} uarch_dma_desc_queue_t;

typedef void (*uarch_dma_engine_callback_t) (uint32_t desc_va, uint32_t va, uint64_t pa, uint32_t len, uint32_t is_read, uint32_t is_desc, void * entry_void);


typedef struct uarch_dma_engine_inst_t {
	uarch_dma_enigne_state_t	state;				///< dpu state
	uarch_dma_desc_queue_t		desc_queue;	 	// fifo containing desc_tracker_entry_t from dma_adapter_insert_to_timing
	uarch_dma_fetch_desc_queue_t desc_fetch_queue;
	bool				fetcherBusy;		///< descriptor fetcher busy (UARCH_DESC_STATE_START)
	bool				decompBusy;			///< decomposition engine busy with a descriptor (UARCH_DESC_STATE_DECOMP thru UARCH_DESC_STATE_DECOMP_DONE)
	bool				flush;				///< quiesce. Used for DMPause.
	uint32_t			tlbLatency;			///< TLB latency. Set @ init time.
	uint32_t			descStartupLatency; ///< This is the latency from the desc fetch's L2_DMA_RData to the first DMA_L2BPR_Cmd_Vld
	uint32_t			desc_active;		// count of descriptor active
	dma_mini_tlb_entry_t	miniTlbE[DMA_TLB_ENTRY_MAX];		///< DMA engine's Mini-TLB
	dma_t				*dma;				///< pointer back to dma engine instance
} uarch_dma_engine_inst_t;

#define uarch_dma_tlbw(...)
#define uarch_dma_tlbinvasid(...)
#define uarch_dma_tlbIsReloadInProgress(...)
#define uarch_dma_engine_step(...)
#define uarch_dma_engine_free(...)
#define uarch_dma_flush_timing(...)
#define uarch_dma_log_to_uarch(...)
#define uarch_dma_log_word_store(...)
#define uarch_dma_snaphot_flush(...)
#define uarch_dma_engine_init(...) (void *)NULL
#define uarch_dma_peek_desc_queue_head_va(...)
#define uarch_dma_tlbLookup(...)

#endif
