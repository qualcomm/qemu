/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 * The User-DMA engine adapter between Hexagon ArchSim and the User-DMA engine.
 */

#ifndef ARCH_DMA_ADAPTER_H_
#define ARCH_DMA_ADAPTER_H_

#include "dma/dma.h"
#include "dma/desc_tracker.h"
#include "external_api.h"


#include "uarch/queue.h"
//#include "arch/uarch/pipequeue.h"


//! Structure to figure out which memory subsystem covers what address range.
#define thread_t CPUHexagonState
struct dma_addr_range_t {
	paddr_t base;    //!> Base address.
	paddr_t size;    //!> Length (size) in bytes.
	void *owner;     //!> An owner memory subsystem to place memory requests.
};


typedef struct dma_access_rights {
	union {
		struct{
			uint32_t x:1;
			uint32_t w:1;
			uint32_t r:1;
			uint32_t u:1;
		};
		uint32_t val;
	};
} dma_access_rights_t;

typedef struct dma_memtype {
	union {
		struct{
			uint32_t vtcm:1;
			uint32_t l2tcm:1;
			uint32_t ahb:1;
			uint32_t l2vic:1;
			uint32_t l1s:1;
			uint32_t l2itcm:1;
			uint32_t axi2:1;
			uint32_t invalid:1;
			uint32_t invalid_cccc:1;
			uint32_t invalid_dma:1;
		};
		uint32_t val;
	};
} dma_memtype_t;

typedef struct dma_memaccess_info {
	uint64_t va;
	paddr_t pa;

	uint32_t jtlb_idx;
	uint64_t jtlb_entry;			///< Raw 64b TLB entry
	uint32_t exception;
	uint32_t access_type;
 	dma_access_rights_t perm;
	dma_memtype_t memtype;
} dma_memaccess_info_t;




//! Structure to support a DMA engine - maintain useful information per thread
//! at the adapter side.

typedef struct dma_adapter_engine_info_t {
    hex_exception_info einfo; //!> Exception information stored for later use.
    thread_t *owner; //!> Owner HW thread to take an exception.

    desc_tracker_t desc_tracker;
    queue_t desc_queue;
    desc_tracker_entry_t *desc_entries[DESC_TABLESIZE];

} dma_adapter_engine_info_t;



//! Function to register an address range that will be used on placing
//! one instance of memory transaction for timing.
//! @param owner Memory subsystem owning a given address range.
//! @param start Start address of a range.
//! @param size Size of the range in bytes.
//! @return 1 for successful registration. 0 for failure.
int dma_adapter_register_addr_range(void *owner, paddr_t start, paddr_t size);

struct dma_addr_range_t* dma_adapter_find_mem(paddr_t paddr);

/* System memory access interface. */
//! @param va Virtual address to translate.
//! @param pa Physical address to get translated.
//! @param perm permissions bit 0: user 1: write: 2: read 3: execute
//! @param memtype 0: L2 space 1: VTCM
//! @param width width of transaction
//! @param store 0 for load type, and non-0 for store type.
//! @return 1 for success, and 0 for exception

uint32_t dma_adapter_xlate_va(dma_t *dma, uint64_t va, uint64_t* pa, dma_memaccess_info_t * dma_mem_access, uint32_t width, uint32_t store, uint32_t extended_va, uint32_t except_vtcm, uint32_t is_dlbc, uint32_t is_forget);
uint32_t dma_adapter_xlate_desc_va(dma_t *dma, uint32_t va, uint64_t* pa, dma_memaccess_info_t * dma_mem_access);

//! @return 1 for success, and 0 for running into exception.
int dma_adapter_memread(dma_t *dma, uint32_t va, uint64_t pa, uint8_t* dst,
                        int width);

//! @return 1 for success, and 0 for running into exception.
int dma_adapter_memwrite(dma_t *dma, uint32_t va, uint64_t pa, uint8_t* src,
                         int width);

//! Initialize the DMA engine when a simulator is invoked.
dma_t *dma_adapter_init(processor_t *proc, int dmanum);

//! Free the DMA engine and resources.
void dma_adapter_free(processor_t *proc, int dmanum);

//! DMA ticking interface to make progress.
void arch_dma_cycle(processor_t *proc, int dmanum);
/// Doesn't do any data transfers
void arch_dma_cycle_no_desc_step(processor_t *proc, int dmanum);

uint32_t arch_dma_enable_timing(processor_t *proc);


//! Single entrance to drive the user-DMA instructions (commands)
//! @param thread Owner thread.
//! @param opcode DMA instruction (command) identifier.
//! @param arg1 dmstart: new descriptor
//              dmlink: tail to append at
//              dmpoll/dmwait: not used
//              dmsyncht: not used
//              dmwaitdescriptor: descriptor
//              dmcfgrd/dmcfgwr: dma number
//              dmpause: not used
//              dmresume: descriptor
//              dmtlbsync: not used
//! @param arg2 dmstart: not used
//              dmlink: new descriptor to append
//              dmpoll/dmwait: not used
//              dmsyncht: not used
//              dmwaitdescriptor: not used
//              dmcfgrd : not used
//              dmcfgwr: data
//              dmpause: not used
//              dmresume: not used
//              dmtlbsync: not used
size4u_t dma_adapter_cmd(thread_t *thread, dma_cmd_t opcode,
		                     size4u_t arg1, size4u_t arg2);



void dma_adapter_tlb_invalidate(thread_t *thread, int tlb_idx, uint64_t tlb_entry_old, uint64_t tlb_entry_new);


int dma_adapter_callback(processor_t *proc, int dma_num, void * dma_xact);

int dma_adapter_register_perm_exception(dma_t *dma, uint32_t va,  dma_access_rights_t access_rights, int store);
int dma_adapter_register_error_exception(dma_t *dma, uint32_t va);


int dma_adapter_descriptor_start(dma_t *dma, uint32_t id, uint32_t desc_va,  uint32_t *desc_info);
int dma_adapter_descriptor_end(dma_t *dma, uint32_t id, uint32_t desc_va,  uint32_t *desc_info, int pause, int exception);

//! Verification testbench step interface
//! Will tick DMA in loop until not no longer running or exception
void arch_dma_tick_until_stop(processor_t *proc, int dmanum);

int dma_adapter_in_monitor_mode(dma_t *dma);
int dma_adapter_in_guest_mode(dma_t *dma);
int dma_adapter_in_user_mode(dma_t *dma);
int dma_adapter_in_debug_mode(dma_t *dma);
FILE * dma_adapter_debug_log(dma_t *dma);
FILE * uarch_dma_adapter_debug_log(dma_t *dma);
int dma_adapter_debug_verbosity(dma_t *dma);

enum {
	DMA_PMU_ACTIVE=0,
	DMA_PMU_STALL_DESC_FETCH,
	DMA_PMU_STALL_SYNC_RESP,
	DMA_PMU_STALL_TLB_MISS,
	DMA_PMU_TLB_MISS,
	DMA_PMU_PAUSE_CYCLES,
	DMA_PMU_DMPOLL_CYCLES,
	DMA_PMU_DMWAIT_CYCLES,
	DMA_PMU_SYNCHT_CYCLES,
	DMA_PMU_TLBSYNCH_CYCLES,
	DMA_PMU_DESC_DONE,
	DMA_PMU_DLBC_FETCH,
	DMA_PMU_DLBC_FETCH_CYCLES,
	DMA_PMU_UNALIGNED_DESCRIPTOR,
	DMA_PMU_ORDERING_DESCRIPTOR,
	DMA_PMU_PADDING_DESCRIPTOR,
	DMA_PMU_UNALIGNED_RD,
	DMA_PMU_UNALIGNED_WR,
	DMA_PMU_COHERENT_RD_CYCLES,
	DMA_PMU_COHERENT_WR_CYCLES,
	DMA_PMU_NONCOHERENT_RD_CYCLES,
	DMA_PMU_NONCOHERENT_WR_CYCLES,
	DMA_PMU_VTCM_RD_CYCLES,
	DMA_PMU_VTCM_WR_CYCLES,
	DMA_PMU_RD_BUFFER_LEVEL_LOW,
	DMA_PMU_RD_BUFFER_LEVEL_HALF,
	DMA_PMU_RD_BUFFER_LEVEL_HIGH,
	DMA_PMU_RD_BUFFER_LEVEL_FULL,
	DMA_PMU_CMD_START,
	DMA_PMU_CMD_LINK,
	DMA_PMU_CMD_RESUME,
	DMA_PMU_NUM};

enum {
	DMA_ACTIVE_ALL_UARCH_TRACE = 0,
	DMA_ACTIVE_RD_UARCH_TRACE = 1,
	DMA_ACTIVE_RD_PENDING_UARCH_TRACE = 2,
};

enum {
	DMA_INSN_LATENCY_DMSTART=0,
	DMA_INSN_LATENCY_DMRESUME,
	DMA_INSN_LATENCY_DMLINK,
	DMA_INSN_LATENCY_DMPAUSE,
	DMA_INSN_LATENCY_DMPOLL,
	DMA_INSN_LATENCY_DMWAIT,
	DMA_INSN_LATENCY_NUM};

void dma_adapter_pmu_increment(dma_t *dma, int event, int increment);
int dma_adapter_get_insn_latency(dma_t *dma, int increment);
void dma_adapter_force_dma_error(thread_t *thread, uint32_t badva, uint32_t syndrome);
void dma_adapter_set_target_descriptor(thread_t *thread, uint32_t dm0, uint32_t va, uint32_t width, uint32_t height, uint32_t no_transfer);

void dma_adapter_active_uarch_callback(dma_t *dma, int lvl);

void dma_adapter_stalled_warning(dma_t *dma);


void dma_adapter_signal_dma_done(dma_t *dma);


desc_tracker_entry_t *  dma_adapter_insert_desc_queue(dma_t *dma, dma_decoded_descriptor_t * desc );
desc_tracker_entry_t * dma_adapter_peek_desc_queue_head(dma_t *dma);
int dma_adapter_pop_desc_queue(dma_t *dma, desc_tracker_entry_t * entry );
int dma_adapter_pop_desc_queue_done(dma_t *dma );
int dma_adapter_pop_desc_queue_pause(dma_t *dma );
int dma_adapter_desc_queue_empty(dma_t *dma);
void dma_adapter_set_dm5(dma_t *dma, uint32_t addr);


uint32_t dma_adapter_get_dmreg(processor_t *proc, uint32_t tnum, uint32_t addr);
void dma_adapter_set_dmreg(processor_t *proc, uint32_t tnum, uint32_t addr, uint32_t val);

static inline thread_t *dma_adapter_retrieve_thread(dma_t *dma)
{
    return ((dma_adapter_engine_info_t *)dma->owner)->owner;
}

uint64_t dma_adapter_get_pcycle(dma_t *dma);
int dma_adapter_match_tlb_entry (dma_t *dma, uint64_t entry, uint32_t asid, uint32_t va );
int dma_adapter_has_extended_tlb(dma_t *dma);
size8u_t dma_adapter_get_pa(dma_t *dma, size8u_t entry, size8u_t vaddr );
void dma_adapter_mask_badva(dma_t *dma, uint32_t mask);


dma_t * dma_adapter_get_dma(processor_t * proc, int dnum);
uint32_t dma_adapter_peek_desc_queue_head_va(dma_t *dma);
int dma_adapter_uwbc_ap_en(dma_t *dma);


void dma_adapter_snapshot_flush(processor_t * proc);

#endif /* ARCH_DMA_ADAPTER_H_ */
