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
 * DMA engine adapter between Hexagon ArchSim and User DMA engine.
 */

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif
#include "cpu.h"
#include "arch.h"
#include "dma_adapter.h"
#include "system.h"

#if 1
//#define PRINTF(DMA, ...)  printf(__VA_ARGS__)
#define PRINTF(...)
#else
#define PRINTF(DMA, ...) \
{ \
	FILE * fp = dma_adapter_debug_log(DMA);\
	if (fp) {\
		fprintf(fp, __VA_ARGS__);\
		fprintf(fp, "\n");\
		fflush(fp);\
	}\
}
#endif

#ifdef VERIFICATION
#define DMA_STALL_SET(STALLTYPE,INSNCHECKER) {                             \
	thread->status |= EXEC_STATUS_REPLAY;                                    \
	thread->staller = staller_dmainsn;                                       \
	thread->stall_type = STALLTYPE;                                          \
	thread->processor_ptr->dma_insn_checker[thread->threadId] = INSNCHECKER; \
	}
#else
#define DMA_STALL_SET(STALLTYPE,INSNCHECKER)
#endif

#define warn(...)
#define INC_TSTAT(a)
#define INC_TSTATN(a,b)
#define CACHE_ASSERT(a,b)
#define ARCH_GET_PCYCLES(a) 0LL
#define sim_err_fatal(...)

#ifdef VERIFICAITON
#define CALL_DMA_CMD(NAME, ...) NAME##_##debug(__VA_ARGS__);
#else
#define CALL_DMA_CMD(NAME, ...) \
	{NAME(__VA_ARGS__); }
#endif

/* defines to reduce compare differences */
#define Regs gpr
#define REG_PC HEX_REG_PC
#define EXCEPT_TYPE_PRECISE           HEX_EVENT_PRECISE
#define PRECISE_CAUSE_PRIV_NO_UWRITE  HEX_CAUSE_PRIV_NO_UWRITE
#define PRECISE_CAUSE_PRIV_NO_WRITE   HEX_CAUSE_PRIV_NO_WRITE
#define PRECISE_CAUSE_PRIV_NO_UREAD   HEX_CAUSE_PRIV_NO_UREAD
#define PRECISE_CAUSE_PRIV_NO_READ    HEX_CAUSE_PRIV_NO_READ


//! Function pointer type of DMA instruction (command) implementation.
typedef size4u_t (*dma_adapter_cmd_impl_t)(thread_t*, size4u_t, size4u_t, dma_insn_checker_ptr*);

// List of the user-DMA instruction (command) implementation functions.
static size4u_t dma_adapter_cmd_start(thread_t *thread, size4u_t new_dma, size4u_t dummy, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_link(thread_t *thread, size4u_t tail, size4u_t new_dma, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_poll(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_wait(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_pause(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_resume(thread_t *thread, size4u_t arg1, size4u_t dummy, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_cfgrd(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_cfgwr(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_syncht(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_tlbsynch(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);

//! Function table of the user-DMA instructions (commands).
// ATTENTION ==================================================================
// The element order should match to that of the enumerator dma_cmd_t
// in src/arch/external_api.h.
// ============================================================================
dma_adapter_cmd_impl_t dma_adapter_cmd_impl_tab[DMA_CMD_UND] = {
	&dma_adapter_cmd_start,             // dmstart.
	&dma_adapter_cmd_link,              // dmlink
	&dma_adapter_cmd_poll,              // dmpoll
	&dma_adapter_cmd_wait,              // dmwait
	&dma_adapter_cmd_syncht,            // dmsyncht
	&dma_adapter_cmd_cfgrd,             // dmcfgrd
	&dma_adapter_cmd_cfgwr,             // dmcfgwr
	&dma_adapter_cmd_pause,             // dmpause
	&dma_adapter_cmd_resume,            // dmresume
	&dma_adapter_cmd_tlbsynch,          // dmtlbsynch
};


//! Address range that can be serviced by the memory subsystem.
struct dma_addr_range_t* dma_addr_tab = NULL;

//! Current entry count of the address range table above.
unsigned int dma_addr_tab_cnt = 0;

//! Comparison function that will be used to sort the address range table.
static int dma_adapter_addrrange_cmp(const void *range_a, const void *range_b) {
	struct dma_addr_range_t *range_a_ptr = (struct dma_addr_range_t*)(range_a);
	struct dma_addr_range_t *range_b_ptr = (struct dma_addr_range_t*)(range_b);

	if ((range_a_ptr->base + range_a_ptr->size) <= range_b_ptr->base) {
		return -1;
	} else if ((range_b_ptr->base + range_b_ptr->size) <= range_a_ptr->base) {
		return 1;
	} else {
		return 0;
	}
}

//! Comparison function that will be used to search a certain address range.
static int dma_adapter_addr_find_cmp(const void *paddrkey, const void *range)
{
	struct dma_addr_range_t *range_ptr = (struct dma_addr_range_t*)(range);
	paddr_t paddr = *((paddr_t *)paddrkey);

	if (paddr < range_ptr->base) {
		return -1;
	} else if (paddr >= (range_ptr->base + range_ptr->size)) {
		return 1;
	} else {
		return 0;
	}
}
uint32_t arch_dma_enable_timing(processor_t *proc) {

	for(int tnum = 0; tnum < proc->runnable_threads_max; tnum++) {
		dma_t *dma = proc->dma[tnum];
		if (dma) {
			dma_set_timing(dma, 1);
		}
	}
	return 0;
}



int dma_adapter_register_addr_range(void* owner, paddr_t base, paddr_t size) {
	struct dma_addr_range_t key;

	key.base = base;
	key.size = size;

	/* Make sure there is no address range already defined */
	if(dma_addr_tab != NULL) {
		if(bsearch(&key, dma_addr_tab, dma_addr_tab_cnt,
		           sizeof(struct dma_addr_range_t), dma_adapter_addrrange_cmp)
		   != NULL) {
			return 0;
		}
	}

	dma_addr_tab = (struct dma_addr_range_t *) realloc(dma_addr_tab,
	                       (dma_addr_tab_cnt + 1) * sizeof(struct dma_addr_range_t));

	dma_addr_tab[dma_addr_tab_cnt] = key;
	dma_addr_tab_cnt++;

	qsort(dma_addr_tab, dma_addr_tab_cnt, sizeof(struct dma_addr_range_t),
	      dma_adapter_addrrange_cmp);

	return 1;
}

struct dma_addr_range_t* dma_adapter_find_mem(paddr_t paddr) {
	if (dma_addr_tab == NULL) { return NULL; }

	struct dma_addr_range_t* mem = (struct dma_addr_range_t *)
		bsearch(&paddr, dma_addr_tab, dma_addr_tab_cnt,
		        sizeof(struct dma_addr_range_t), dma_adapter_addr_find_cmp);

	return mem;
}

#if 0
static inline thread_t* dma_adapter_retrieve_thread(dma_t *dma) {
	return ((dma_adapter_engine_info_t *)dma->owner)->owner;
}
#endif
thread_t* dma_adapter_retrieve_thread(dma_t *dma) {
	return ((dma_adapter_engine_info_t *)dma->owner)->owner;
}

static inline dma_adapter_engine_info_t* dma_retrieve_dma_adapter(dma_t *dma) {
	return (dma_adapter_engine_info_t *)dma->owner;
}


//! Function to request stalling. The DMA engine calls this function to stall
//! an instruction stream.
static int dma_adapter_set_staller(dma_t *dma, dma_insn_checker_ptr insn_checker) {
	thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
#ifdef VERIFICATION
	PRINTF(dma, "DMA %d Tick %8lli: ADAPTER  dma_adapter_set_staller. ", dma->num,  (long long int)dma_get_tick_count(dma));
#endif
	// Set up a staller per thread.
	DMA_STALL_SET(dmainsn, insn_checker);

	return 0;
}

int dma_adapter_has_extended_tlb(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	return thread->processor_ptr->arch_proc_options->QDSP6_DMA_EXTENDED_VA_PRESENT>0;
}

int dma_adapter_in_monitor_mode(dma_t *dma) {
#ifdef CONFIG_USER_ONLY
  return 0;
#else
	return sys_in_monitor_mode(dma_adapter_retrieve_thread(dma));
#endif
}
int dma_adapter_in_guest_mode(dma_t *dma) {
#ifdef CONFIG_USER_ONLY
  return 0;
#else
	return sys_in_guest_mode(dma_adapter_retrieve_thread(dma));
#endif
}
int dma_adapter_in_user_mode(dma_t *dma) {
#ifdef CONFIG_USER_ONLY
  return 1;
#else
	return sys_in_user_mode(dma_adapter_retrieve_thread(dma));
#endif
}
int dma_adapter_in_debug_mode(dma_t *dma) {
#ifdef VERIFICATION
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	if (fIN_DEBUG_MODE(thread->threadId)) {
		PRINTF(dma, "DMA %d: ADAPTER thread->debug_mode=%d ISDST=%llx ",dma->num, thread->debug_mode, (fREAD_GLOBAL_REG_FIELD(ISDBST,ISDBST_DEBUGMODE) & 1<<thread->threadId));
	}
#endif
	return fIN_DEBUG_MODE(thread->threadId);
}

FILE * dma_adapter_debug_log(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	return thread->processor_ptr->arch_proc_options->dmadebugfile;
}
FILE * uarch_dma_adapter_debug_log(dma_t *dma) {
	return (FILE *)0;}
int dma_adapter_debug_verbosity(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	return thread->processor_ptr->arch_proc_options->dmadebug_verbosity;
}


void do_callback(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int callback_state);
void do_callback(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int callback_state) {
#if 0

	thread_t * thread = dma_adapter_retrieve_thread(dma);

	dma_descriptor_callback_info_t info = {0};
	info.dmano = dma->num;
	info.desc_va = desc_va;
	info.callback_state = (dma_callback_state_t)callback_state;
	info.type = (uint32_t)get_dma_desc_desctype((void*)desc_info);
	info.state = (uint32_t)get_dma_desc_dstate((void*)desc_info);
	info.src_va = (uint32_t)get_dma_desc_src((void*)desc_info);
	info.dst_va = (uint32_t)get_dma_desc_dst((void*)desc_info);
	info.desc_id = id;
	// Translate a VA.
	xlate_info_t xlate_task;   // Storage to get a PA returned back.
	hex_exception_info e_info;     // Storage to retrieve an exception if it occurs.

	sys_xlate(thread, desc_va, TYPE_LOAD, access_type_load, 0, 4-1, &xlate_task, &e_info);
	info.desc_pa = xlate_task.pa;

	sys_xlate(thread, info.src_va, TYPE_LOAD, access_type_load, 0, 4-1, &xlate_task, &e_info);
	info.src_pa = xlate_task.pa;

	sys_xlate(thread, info.dst_va, TYPE_STORE, access_type_store, 0, 4-1, &xlate_task, &e_info);
	info.dst_pa = xlate_task.pa;

	if (info.type == 0) {
		info.len = (uint32_t)get_dma_desc_length((void*)desc_info);
	} else {
		info.src_offset = (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info);
		info.dst_offset = (uint32_t)get_dma_desc_dstwidthoffset((void*)desc_info);
		info.hlen = (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info) - (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info);
		info.vlen = (uint32_t)get_dma_desc_roiheight((void*)desc_info);

		info.desc2d_type =  (uint32_t)((HEXAGON_DmaDescriptor2D_t*)desc_info)->type;
		info.src_upper = (uint32_t)((HEXAGON_DmaDescriptor2D_t*)desc_info)->srcUpperAddr;
		info.dst_upper = (uint32_t)((HEXAGON_DmaDescriptor2D_t*)desc_info)->dstUpperAddr;
	}

	if (thread->processor_ptr->options->dma_callback) {
			thread->processor_ptr->options->dma_callback(thread->system_ptr, thread->processor_ptr, &info);
	}
#endif
}

void dma_adapter_set_target_descriptor(thread_t *thread, uint32_t dm0,  uint32_t va, uint32_t width, uint32_t height, uint32_t no_transfer) {
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	CALL_DMA_CMD(dma_target_desc,dma, va, width, height, no_transfer);
}


int dma_adapter_match_tlb_entry (dma_t *dma, uint64_t entry, uint32_t asid, uint32_t va ) {
	return 0;
}


int dma_adapter_descriptor_start(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info) {
	//PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_descriptor_start : desc_va = 0x%x id=%d", dma->num, desc_va, id);
	do_callback(dma, id, desc_va, desc_info, DMA_DESC_STATE_START);
	return 0;
}

int dma_adapter_descriptor_end(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int pause, int exception) {
	//PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_descriptor_end : desc_va = 0x%x id=%d", dma->num, desc_va, id);
	int callback_state = DMA_DESC_STATE_DONE;
	callback_state = (pause) ? DMA_DESC_STATE_PAUSE : callback_state;
	callback_state = (exception) ? DMA_DESC_STATE_EXCEPT : callback_state;
	do_callback(dma, id, desc_va, desc_info, callback_state);
	return 0;
}


uint32_t dma_adapter_xlate_desc_va(dma_t *dma, uint32_t va, uint64_t* pa, dma_memaccess_info_t * dma_memaccess_info) {

	// Translate a VA.
	xlate_info_t xlate_task;   // Storage to get a PA returned back.
        hex_exception_info e_info = {
            0
        }; // Storage to retrieve an exception if it occurs.
        thread_t * thread = dma_adapter_retrieve_thread(dma);

	uint32_t ret = sys_xlate_dma(thread, (uint32_t)va, TYPE_DMA_FETCH,  access_type_load,  0, 0, &xlate_task, &e_info, 0, 0, 0, 0);

	(*pa) = xlate_task.pa;
	dma_access_rights_t * perm = &dma_memaccess_info->perm;
	perm->val = 0;
	perm->u = xlate_task.pte_u;
	perm->w = xlate_task.pte_w;
	perm->r = xlate_task.pte_r;
	perm->x = xlate_task.pte_x;

	dma_memtype_t * memtype = &dma_memaccess_info->memtype;
	memtype->vtcm = xlate_task.memtype.vtcm;
	memtype->l2tcm = xlate_task.memtype.l2tcm;
	memtype->ahb = xlate_task.memtype.ahb;
	memtype->axi2 = xlate_task.memtype.axi2;
	memtype->l2vic = xlate_task.memtype.l2vic;
	memtype->invalid_cccc = xlate_task.memtype.invalid_cccc;
	memtype->invalid_dma = xlate_task.memtype.invalid_dma || xlate_task.memtype.invalid_cccc;

	dma_memaccess_info->exception = (ret == 0);
#if 0
	dma_memaccess_info->jtlb_idx   = dma_memaccess_info->exception ? -1 : xlate_task.jtlbidx;
	dma_memaccess_info->jtlb_entry = dma_memaccess_info->exception ? 0 : thread->processor_ptr->TLB_regs[xlate_task.jtlbidx];
#endif

	dma_memaccess_info->pa = *pa;
	dma_memaccess_info->va = (uint64_t)va;
	dma_memaccess_info->access_type = TYPE_DMA_FETCH;
	if (ret == 0) {
		dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
		dma_info->einfo = e_info;

		warn("DMA %d: ADAPTER  exception on translation of descriptor for VA=%x badva0=%x badva1=%x cause=%x cccc=%x U:%x R:%x W:%x X:%x jtlbidx=%d", dma->num, va, dma_info->einfo.badva0, dma_info->einfo.badva1, dma_info->einfo.cause, xlate_task.cccc, perm->u, perm->r, perm->w, perm->x, xlate_task.jtlbidx);
		PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER exception on descriptor fetch=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d va=0x%x PA=%llx", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), dma_info[dma->num].einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma, (unsigned int)va, (long long int)pa);
	}



	return ret;
}

void dma_adapter_mask_badva(dma_t *dma, uint32_t mask) {
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	dma_info->einfo.badva0 &= mask;
}


uint32_t dma_adapter_xlate_va(dma_t *dma, uint64_t va, uint64_t* pa, dma_memaccess_info_t * dma_memaccess_info, uint32_t width, uint32_t store, uint32_t extended_va, uint32_t except_vtcm, uint32_t is_dlbc, uint32_t is_forget) {

	// Decode Access type

	uint32_t access_type = TYPE_LOAD;
	uint32_t maptr_type = access_type_load;
	if (store) {
		access_type = TYPE_STORE;
		maptr_type = access_type_store;
	}

	// Translate a VA.
	xlate_info_t xlate_task;   // Storage to get a PA returned back.
        hex_exception_info e_info = {
            0
        }; // Storage to retrieve an exception if it occurs.
        thread_t * thread = dma_adapter_retrieve_thread(dma);

	uint32_t ret = sys_xlate_dma(thread, va, access_type, maptr_type, 0, width-1, &xlate_task, &e_info, extended_va, except_vtcm, is_dlbc, is_forget);
	// If an exception occurs, keep it for future.
	// Retrieve a PA.

	(*pa) = xlate_task.pa;

	dma_access_rights_t * perm = &dma_memaccess_info->perm;
	perm->val = 0;
	perm->u = xlate_task.pte_u;
	perm->w = xlate_task.pte_w;
	perm->r = xlate_task.pte_r;
	perm->x = xlate_task.pte_x;

	dma_memtype_t * memtype = &dma_memaccess_info->memtype;
	memtype->vtcm = xlate_task.memtype.vtcm;
	memtype->l2tcm = xlate_task.memtype.l2tcm;
	memtype->ahb = xlate_task.memtype.ahb;
	memtype->axi2 = xlate_task.memtype.axi2;
	memtype->l2vic = xlate_task.memtype.l2vic;
	memtype->invalid_cccc = xlate_task.memtype.invalid_cccc;
	memtype->invalid_dma = xlate_task.memtype.invalid_dma || xlate_task.memtype.invalid_cccc;

	dma_memaccess_info->exception = (ret == 0);
#if 0
	dma_memaccess_info->jtlb_idx   = dma_memaccess_info->exception ? -1 : xlate_task.jtlbidx;
	dma_memaccess_info->jtlb_entry = dma_memaccess_info->exception ? 0 : thread->processor_ptr->TLB_regs[xlate_task.jtlbidx];
#endif

	dma_memaccess_info->pa = *pa;
	dma_memaccess_info->va = va;
	dma_memaccess_info->access_type = access_type;

	memtype->invalid_dma |= (memtype->vtcm || memtype->l2tcm) && is_dlbc;

	if (ret == 0) {
		dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
		dma_info->einfo = e_info;

		PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER %s Exception Encountered: VA=0x%llx (38-bit=%x) PA=0x%llx size=%d. cause=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d U:%x R:%x W:%x X:%x",dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), (store) ? "WR" : "RD",
		(long long int)va, extended_va, (long long int)*pa, width, dma_info->einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma,
		perm->u, perm->w, perm->r, perm->x);

		warn("DMA %d: ADAPTER %s Exception Encountered: VA=0x%llx (38-bit=%x) PA=0x%llx size=%d cause=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d U:%x R:%x W:%x X:%x",dma->num, (store) ? "WR" : "RD",
		(long long int)va, extended_va, (long long int)*pa, width, dma_info->einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma,
		perm->u, perm->w, perm->r, perm->x);


		if(extended_va) {
			dma_memaccess_info->va = GET_DMA_LDST_ERROR_BADVA(extended_va, va);
		}

	}
	//else
	//{
	//	PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER using tlb entry=%d memtype->vtcm=%d excpet_vtcm=%d invalid=%d",  dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), xlate_task.jtlbidx, memtype->vtcm,except_vtcm, memtype->invalid_dma);
    //
	//}



	return ret;
}
int dma_adapter_register_error_exception(dma_t *dma, uint32_t va) {
#if 0
	thread_t * thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	hex_exception_info e_info;
	register_dma_error_exception(thread, &e_info, va);
	dma_info->einfo = e_info;
	warn("DMA %d: Registering DMA error exception cause=%x badva=%x", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), e_info.cause, va);
#endif
	return 0;
};

void dma_adapter_force_dma_error(thread_t *thread, uint32_t badva, uint32_t syndrome) {
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_force_error(dma, badva, syndrome);
	//PRINTF(dma, "DMA %d: ADAPTER  forcing DMA error for badva=%x syndrome=%d", dma->num, badva, syndrome);
	//warn("DMA %d: ADAPTER  forcing DMA error for badva=%x syndrome=%d", dma->num, badva, syndrome);
}

int dma_adapter_register_perm_exception(dma_t *dma, uint32_t va,  dma_access_rights_t access_rights, int store) {

	thread_t * thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
        hex_exception_info e_info;

        e_info.valid = 1;
	e_info.type = EXCEPT_TYPE_PRECISE;


	if (store) {
		e_info.cause = (access_rights.w)  ?  PRECISE_CAUSE_PRIV_NO_UWRITE : PRECISE_CAUSE_PRIV_NO_WRITE;
	} else {
		e_info.cause =  (access_rights.r) ?  PRECISE_CAUSE_PRIV_NO_UREAD : PRECISE_CAUSE_PRIV_NO_READ;
	}

	e_info.badva0 = va;
#if !defined(CONFIG_USER_ONLY)
	e_info.badva1 = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA1);
#endif
	e_info.bv0 = 1;
	e_info.bv1 = 0;
	e_info.bvs = 0;

	e_info.elr = thread->Regs[REG_PC];
	e_info.diag = 0;
	e_info.de_slotmask = 1;

	dma_info->einfo = e_info;
	warn("DMA %d: Registering permissions exception cause=%x access_writes= U:%x R:%x W:%x X:%x badva=%x", dma->num, e_info.cause, access_rights.u, access_rights.r, access_rights.w, access_rights.x, va );

	return 0;
};

void dma_adapter_stalled_warning(dma_t *dma) {
	thread_t * thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
	uint32_t val=0;
	dma_cmd_cfgrd(dma, 2, &val, NULL);
	warn("DMA %d: Decomposition stalled due to DM2 Guest/Monitor mode stall setting DM2=0x%08x\n", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), val);
}

#ifdef CONFIG_USER_ONLY
static size8u_t memread(thread_t *thread, mem_access_info_t *memaptr, int do_trace)
{
    paddr_t paddr = memaptr->paddr;
    int width = memaptr->width;
    size8u_t data;

    switch (width) {
        case 1:
            data = cpu_ldub_data_ra(thread, paddr, GETPC());
            break;
        case 2:
            data = cpu_lduw_data_ra(thread, paddr, GETPC());
            break;
        case 4:
            data = cpu_ldl_data_ra(thread, paddr, GETPC());
            break;
        case 8:
            data = cpu_ldq_data_ra(thread, paddr, GETPC());
            break;
        default:
            g_assert_not_reached();
    }
    return data;
}

static void memwrite(thread_t *thread, mem_access_info_t *memaptr)
{
    paddr_t paddr = memaptr->paddr;
    int width = memaptr->width;
    size8u_t data = memaptr->stdata;

    switch (width) {
        case 1:
            cpu_stb_data_ra(thread, paddr, data, GETPC());
            break;
        case 2:
            cpu_stw_data_ra(thread, paddr, data, GETPC());
            break;
        case 4:
            cpu_stl_data_ra(thread, paddr, data, GETPC());
            break;
        case 8:
            cpu_stq_data_ra(thread, paddr, data, GETPC());
            break;
        default:
            g_assert_not_reached();
    }
}

int dma_adapter_memread(dma_t *dma, uint32_t va, uint64_t pa, uint8_t *dst, int width) {
	mem_access_info_t macc_task;
	thread_t * thread = dma_adapter_retrieve_thread(dma);
	macc_task.vaddr = va;
	macc_task.paddr = pa;
	macc_task.width = 1;
  while (width > 0) {
    (*dst) = memread(thread, &macc_task, 0);
 #ifdef VERIFICATION
 		//PRINTF(dma, "DMA ADAPTER:  %d: callback for DREAD at %llx byte=%x",thread->threadId, macc_task.paddr, (*dst));
		if (thread->processor_ptr->options->sim_memory_callback && thread->processor_ptr->options->testgen_mode) {
			//PRINTF(dma, "DMA ADAPTER:  %d: callback for DREAD at %llx byte=%x",thread->threadId, macc_task.paddr, (*dst));
    		thread->processor_ptr->options->sim_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId,macc_task.vaddr,macc_task.paddr,1,DREAD,(*dst));
  		}
#endif
    macc_task.vaddr++;
    macc_task.paddr++;
    dst++;
    width--;

  }

//	return ((*dst) != 0xdeadbeef);
	return 1;
}

int dma_adapter_memwrite(dma_t *dma, uint32_t va, uint64_t pa, uint8_t *src, int width) {
	mem_access_info_t macc_task;
	thread_t * thread = dma_adapter_retrieve_thread(dma);
	macc_task.vaddr = va;
	macc_task.paddr = pa;
	macc_task.width = 1;

	while (width > 0) {
		macc_task.stdata = (*src);
		memwrite(thread, &macc_task);
#ifdef VERIFICATION
		//PRINTF(dma, "DMA ADAPTER:  %d: callback for DWRITE at %llx byte=%x",thread->threadId, macc_task.paddr, (*src));
		if (thread->processor_ptr->options->sim_memory_callback && thread->processor_ptr->options->testgen_mode) { \
			//PRINTF(dma, "DMA ADAPTER:  %d: callback for DWRITE at %llx byte=%x",thread->threadId, macc_task.paddr, (*src));
    	thread->processor_ptr->options->sim_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId,macc_task.vaddr,macc_task.paddr,1,DWRITE,(*src));
  	}
#endif
		macc_task.vaddr++;
		macc_task.paddr++;
		src++;
		macc_task.stdata = (*src);
		width--;
	}
	return 1;
}
#endif

dma_t *dma_adapter_init(processor_t *proc, int dmanum) {
	dma_t *dma = NULL;
	dma_adapter_engine_info_t *dma_adapter = NULL;

	if ((dma = (dma_t *) calloc(1, sizeof(dma_t))) == NULL) {
		sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
		              __FILE__, __LINE__, "cannot create dma");
		return NULL;
	}

	if ((dma_adapter = (dma_adapter_engine_info_t *) calloc(1, sizeof(dma_adapter_engine_info_t))) == NULL) {
		sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
		              __FILE__, __LINE__, "cannot create dma_adapter");
		return NULL;
	}



  // Set up other arguments.
	dma->num = dmanum;

	// An owner of each DMA instance is a hardware thread.
  	dma_adapter->owner = proc->thread[dmanum];
	dma->owner = (void*)dma_adapter;
	proc->dma_engine_info[dmanum] = (struct dma_adapter_engine_info *)dma_adapter;
	desc_tracker_init(proc, dmanum);

	queue_alloc(proc, &dma_adapter->desc_queue, dma_adapter->desc_entries, DESC_TABLESIZE);
  	// At a thread side, we will maintain an instruction completeness checker
  	// to manage stalling. Here, we initialize the checker to null by default.
  	proc->dma_insn_checker[dmanum] = NULL;

  if (dma_init(dma, proc->thread[dmanum]->timing_on) == 0) {
		sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
		              __FILE__, __LINE__, "failed dma_init");

		free(dma);
		free(dma_adapter);
		return NULL;
	}


	return dma;
}
void dma_adapter_tlb_invalidate(thread_t *thread, int tlb_idx, uint64_t tlb_entry_old, uint64_t tlb_entry_new) {
	//dma_t *dma = NULL; // This is a global invalidate so it should go to all DMA's
	//uint32_t tlb_asid = GET_FIELD(PTE_ASID, tlb_entry_old);

    g_assert(thread->processor_ptr->runnable_threads_max > 0);
	// On TLB Write, single the dma's that the tlbw entry has been erased.
	// timing model will flush it self if it can't translate
	//if (proc->timing_on) {
		dma_t *dma = thread->processor_ptr->dma[0];
		//PRINTF(dma, "DMA X: Tick %8lli: TLBW invalidate for tlb[%d]=%llx new entry: %llx from tnum=%d", ARCH_GET_PCYCLES(thread->processor_ptr), tlb_idx, (long long int)tlb_entry_old, (long long int)tlb_entry_new,   thread->threadId);
		for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
			dma = thread->processor_ptr->dma[tnum];

			if (thread->processor_ptr->timing_on) {
				dma_arch_tlbw(dma, tlb_idx);
			}
			CALL_DMA_CMD(dma_retry_after_exception,dma);
		}
	//}
}

desc_tracker_entry_t *  dma_adapter_insert_desc_queue(dma_t *dma, dma_decoded_descriptor_t * desc ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	desc_tracker_entry_t * entry = desc_tracker_acquire(proc, dma->num, desc);
	int idx = queue_push(proc, dma->num, &dma_info->desc_queue);
	dma_info->desc_entries[idx] = entry;
	entry->dnum = dma->num;
	entry->desc = *desc;
	entry->dma = (void *) dma;
	return entry;
}



int dma_adapter_pop_desc_queue(dma_t *dma, desc_tracker_entry_t * entry ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_pop(proc, dma->num, &dma_info->desc_queue);
	CACHE_ASSERT(proc, dma_info->desc_entries[idx]->id == entry->id);
	PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER pop from descriptor tracker entry=idx=%d id=%lli", dma->num, ARCH_GET_PCYCLES(proc), idx, entry->id);
	dma_info->desc_entries[idx] = NULL;
	desc_tracker_release(proc, dma->num, entry);
	return 1;
}
uint64_t dma_adapter_get_pcycle(dma_t *dma){
	thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
	return ARCH_GET_PCYCLES(thread->processor_ptr);
}



desc_tracker_entry_t *   dma_adapter_peek_desc_queue_head(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
	//PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_peek_desc_queue_head idx=%d", dma->num, idx);
	return (idx >= 0) ? dma_info->desc_entries[idx] : 0;
}

uint32_t dma_adapter_peek_desc_queue_head_va(dma_t *dma) {
	desc_tracker_entry_t * entry = dma_adapter_peek_desc_queue_head(dma);
	return (entry) ? entry->desc.va : 0;
}

int dma_adapter_pop_desc_queue_done(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int done = 1;
	int is_empty = 0;
	while (done) {
		int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
		int pop = 0;
		pop = (idx>=0);
		if ( pop ) {
			pop &= (dma_info->desc_entries[idx]->desc.state == DMA_DESC_DONE) || (dma_info->desc_entries[idx]->desc.state ==  DMA_DESC_EXCEPT_ERROR);
		}
		// If it's exception running, we're going to keep that descriptor
		if ( pop ) {
			idx = queue_pop(proc, dma->num, &dma_info->desc_queue);

			PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER releasing descriptor va=%x id=%lli desc.state=%d pause=%d", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, dma_info->desc_entries[idx]->id, dma_info->desc_entries[idx]->desc.state,  dma_info->desc_entries[idx]->desc.pause);


			// Update Descriptor
			CALL_DMA_CMD(dma_update_descriptor_done,dma, &dma_info->desc_entries[idx]->desc);

			desc_tracker_release(proc, dma->num, dma_info->desc_entries[idx]);
			dma_info->desc_entries[idx] = NULL;

			is_empty = queue_empty(proc, dma->num, &dma_info->desc_queue);
		} else {

			//if (dma_info->desc_entries[idx]->desc.state ==  DMA_DESC_EXCEPT_RUNNING) {
			//	PRINTF(dma, "DMA %d: Tick: %lli ADAPTER not releasing descriptor va=%x id=%lli desc.state=%d due to exception, but updating", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, dma_info->desc_entries[idx]->id, dma_info->desc_entries[idx]->desc.state);
			//	dma_update_descriptor_done(dma, &dma_info->desc_entries[idx]->desc);
			//}

			done = 0;
		}

	}


	if (is_empty && !dma_in_error(dma)) {
		PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER stopping DMA since we are done with all the descriptors in the chain.", dma->num, ARCH_GET_PCYCLES(proc));
		CALL_DMA_CMD(dma_stop,dma);
	} else if (is_empty && !dma_in_error(dma)){
		PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER. We're done with all the descriptors in chain, but in exception mode. can't go to idle.", dma->num, ARCH_GET_PCYCLES(proc));
	}

	return is_empty;
}

int dma_adapter_pop_desc_queue_pause(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int done = 1;
	int is_empty = 0;
	while (done) {
		int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
		int pop = 0;
		pop = (idx>=0);
		if ( pop ) {
			idx = queue_pop(proc, dma->num, &dma_info->desc_queue);
			PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER cmd_pause releasing descriptor va=%x id=%lli desc.state=%d pause=%d", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, dma_info->desc_entries[idx]->id, dma_info->desc_entries[idx]->desc.state,  dma_info->desc_entries[idx]->desc.pause);

			// Update Descriptor
			CALL_DMA_CMD(dma_update_descriptor_done,dma, &dma_info->desc_entries[idx]->desc);
			desc_tracker_release(proc, dma->num, dma_info->desc_entries[idx]);
			dma_info->desc_entries[idx] = NULL;
			is_empty = queue_empty(proc, dma->num, &dma_info->desc_queue);
		} else {
			done = 0;
		}

	}
	if (is_empty && !dma_in_error(dma)) {
		CALL_DMA_CMD(dma_stop,dma);
	}

	return is_empty;
}


int dma_adapter_desc_queue_empty(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	return queue_empty(proc, dma->num, &dma_info->desc_queue);
}



uint32_t dma_adapter_head_desc_queue_peek(dma_t *dma);
uint32_t dma_adapter_head_desc_queue_peek(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
	PRINTF(dma, "DMA %d: Tick %lli: ADAPTER  dma_adapter_head_desc_queue_peek idx=%d", dma->num, ARCH_GET_PCYCLES(proc), idx);
	return (idx >= 0) ? dma_info->desc_entries[idx]->desc.va : 0;
}



void dma_adapter_free(processor_t *proc, int dmanum) {
	dma_free(proc->dma[dmanum]);
	free(proc->dma[dmanum]);
	desc_tracker_free(proc, dmanum);
	free(proc->dma_engine_info[dmanum]);
	proc->dma[dmanum] = NULL;
}

void arch_dma_cycle(processor_t *proc, int dmanum) {
	dma_t *dma = proc->dma[dmanum];
	CALL_DMA_CMD(dma_tick, dma, 1);

}

void arch_dma_cycle_no_desc_step(processor_t *proc, int dmanum) {
	dma_t *dma = proc->dma[dmanum];
	CALL_DMA_CMD(dma_tick,dma, 0);
	desc_tracker_cycle(proc, dmanum);
}
void arch_dma_tick_until_stop(processor_t *proc, int dmanum) {
	dma_t *dma = proc->dma[dmanum];
	uint32_t status = 0;
	uint32_t ticks = 0;
	while(!status) {
#ifdef VERIFICATION
		status = dma_tick_debug(dma, 1);	// status == 0 running, status == 1, stopped for some reason
#else
			status = dma_tick(dma, 1);
#endif
		ticks++;
	}

#ifdef VERIFICATION
	thread_t * thread=proc->thread[0]; // for warn
	warn("DMA %d: ADAPTER  arch_verif_dma_step ticks=%u", dmanum, ticks);
	PRINTF(dma, "DMA %d: Tick %lli: ADAPTER arch_verif_dma_step ticks=%u", dma->num, ARCH_GET_PCYCLES(proc), ticks);
#endif

}





//! Report an exception happened to an owner thread.
static int dma_adapter_report_exception(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);

	warn("DMA %d: ADAPTER  report_exception PC=%x Syndrome=%d for badva=%x", dma->num, thread->Regs[REG_PC], dma->error_state_reason, dma_info->einfo.badva0);
	PRINTF(dma, "DMA %d: Tick %8lli: ADAPTER  report_exception for badva=%x",dma->num, thread->processor_ptr->monotonic_pcycles, dma_info->einfo.badva0 );

	// If an exception occurred while DMA operations just after DMSTART
	// in the same tick, ELR is set to a PC of DMSTART. Thus, later after
	// the exception is resolved, we come to hit the DMSTART again, which is
	// not correct. Since the exception is exposed to DMPOLL or DMWAIT,
	// elr should be adjusted to be a PC of DMPOLL or DMWAIT.
	dma_info->einfo.elr = thread->Regs[REG_PC];
#if !defined(CONFIG_USER_ONLY)
	dma_info->einfo.badva1 = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA1);
#endif

	// Take an owner thread an exception.
	//register_einfo(thread, &dma_info->einfo);

	INC_TSTAT(tudma_tlb_miss);

	return 0;
}




//! dmstart implementation.
size4u_t dma_adapter_cmd_start(thread_t *thread, size4u_t new_dma, size4u_t dummy,
                               dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	dma->pc = thread->Regs[REG_PC];

	CALL_DMA_CMD(dma_cmd_start, dma, new_dma, &report);

	if (report.excpt == 1) {
		// If the command ran into an exception, we have to report it to an owner
		// thread, so that the command can be replayed again later.
		dma_adapter_report_exception(dma);
	} else {
		if (new_dma != 0) {
#ifndef CONFIG_USER_ONLY
            uint32_t ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
			fINSERT_BITS(ssr, reg_field_info[SSR_ASID].width,
                reg_field_info[SSR_ASID].offset,
                (GET_SSR_FIELD(SSR_ASID, ssr)));
            ARCH_SET_SYSTEM_REG(thread, HEX_SREG_SSR, ssr);
#else
            g_assert_not_reached();
#endif
		}
	}

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;
	return 0;
}

//! dmlink and its variants' implementation.
size4u_t dma_adapter_cmd_link(thread_t *thread, size4u_t tail, size4u_t new_dma, dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	dma->pc = thread->Regs[REG_PC];

	CALL_DMA_CMD(dma_cmd_link, dma, tail, new_dma, thread->mem_access[0].paddr, thread->mem_access[0].xlate_info.memtype.invalid_dma, &report);

	if (report.excpt == 1) {
		// If the command ran into an exception, we have to report it to an owner
		// thread, so that the command can be replayed again later.
		dma_adapter_report_exception(dma);
	} else {
		if (new_dma != 0) {
#ifndef CONFIG_USER_ONLY
            uint32_t ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
			fINSERT_BITS(ssr, reg_field_info[SSR_ASID].width,
                reg_field_info[SSR_ASID].offset,
                (GET_SSR_FIELD(SSR_ASID, ssr)));
            ARCH_SET_SYSTEM_REG(thread, HEX_SREG_SSR, ssr);
#else
            g_assert_not_reached();
#endif
		}
	}

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	return 0;
}

//! dmpoll implementation.
size4u_t dma_adapter_cmd_poll(thread_t *thread, size4u_t dummy1,
                              size4u_t dummy2,
                              dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	size4u_t dst=0;   // Destination to get DM0 value returned back.

#ifdef VERIFICATION
	if (thread->dmpoll_hint_valid) {
		thread->dmpoll_hint_valid = 0;
		warn("DMA %d: ADAPTER dma_poll thread=%u using hint value of %08x instead of calling dma model",  dma->num, thread->threadId, thread->dmpoll_hint_val);
		return thread->dmpoll_hint_val;
	}
#endif

	CALL_DMA_CMD(dma_cmd_poll, dma, &dst, &report);

	if (report.excpt == 1) {
		dma_adapter_report_exception(dma);
		PRINTF(dma, "DMA %d: Tick %lli: ADAPTER  retry after exception report", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr));
		CALL_DMA_CMD(dma_retry_after_exception,dma);
	}

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	return dst;
}

//! dmwait implementation.
size4u_t dma_adapter_cmd_wait(thread_t *thread, size4u_t dummy1,
                              size4u_t dummy2,
                              dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	size4u_t dst=0;

	// When a new exception occurred, we already had been in
	// "RUNNING | EXCEPTION | PAUSED". This command call will clear "EXCEPTION".
	// After the exception is resolved, we will revisit this command again.
	// Then, the command call below will clear "PAUSED" too, so that we can move
	// forward.
	CALL_DMA_CMD(dma_cmd_wait, dma, &dst, &report);

	if (report.excpt == 1) {
		// If we reach here, it means we had a new exception, and the command call
		// above cleared "EXCEPTION". Now, we have to report the exception
		// to an owner thread.
		dma_adapter_report_exception(dma);

		// When we reach here, we expect "RUNNING | PAUSED".
		// Later, an exception handler will be brought up, and thanks to replaying,
		// we will hit this command again. But, the DMA ticking interface will be
		// skipped - it will filter out "PAUSED" status not to run.
		// When we revisit this command again by the replying, we will hit the
		// clause below which will set up a staller while waiting for a DMA task
		// completed.
		CALL_DMA_CMD(dma_retry_after_exception,dma);
	}

	(*insn_checker) = report.insn_checker;
	return dst;
}



//! dmpause implementation.
size4u_t dma_adapter_cmd_pause(thread_t *thread, size4u_t dummy1,
                               size4u_t dummy2,
                               dma_insn_checker_ptr *insn_checker) {

	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	size4u_t dst=0;

	CALL_DMA_CMD(dma_cmd_pause, dma, &dst, &report);

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	if (report.excpt) {
		dma_adapter_pop_desc_queue_pause(dma);
	}
	return dst;
}

//! dmresume implementation.
size4u_t dma_adapter_cmd_resume(thread_t *thread, size4u_t arg1, size4u_t dummy,
                                dma_insn_checker_ptr *insn_checker) {

	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};

	if ((arg1& ~0x3)==0) {
		warn("DMA %d: dmresume with Rs32=%x is a nop", dma->num, arg1);
	}

	CALL_DMA_CMD(dma_cmd_resume, dma, arg1, &report);

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	return 0;
}

uint32_t dma_adapter_get_dmreg(processor_t *proc, uint32_t tnum, uint32_t addr) {
	thread_t * thread = proc->thread[tnum];
	return dma_adapter_cmd_cfgrd(thread, addr, 0, NULL);
}
void dma_adapter_set_dmreg(processor_t *proc, uint32_t tnum, uint32_t addr, uint32_t val) {
	thread_t * thread = proc->thread[tnum];
	if(thread == NULL) return;
	dma_adapter_cmd_cfgwr(thread, addr, val, NULL);
}
size4u_t dma_adapter_cmd_cfgrd(thread_t *thread,
                               size4u_t index,
                               size4u_t dummy2,
                               dma_insn_checker_ptr *insn_checker)
{
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
    uint32_t val=0;

	index &= 0xf; // QDSP-38444
    CALL_DMA_CMD(dma_cmd_cfgrd, dma, index, &val, &report);

	// We should relay the instruction completeness checker.
	if (insn_checker)
		(*insn_checker) = report.insn_checker;

	warn("DMA %d: ADAPTER  dma_cfgrd dm%d for thread=%u val=%x", dma->num, index, thread->threadId, val);
    return val;
}

void dma_adapter_snapshot_flush(processor_t * proc) {
    g_assert(proc->runnable_threads_max > 0);
	for (int32_t tnum = 0; tnum < proc->runnable_threads_max; tnum++) {
		thread_t * thread = proc->thread[tnum];
		dma_t *dma = thread->processor_ptr->dma[tnum];
		uarch_dma_snaphot_flush(dma);
		dma_adapter_pop_desc_queue_pause(dma);
	}
}

size4u_t dma_adapter_cmd_cfgwr(thread_t *thread,
                               size4u_t index,
                               size4u_t val,
                               dma_insn_checker_ptr *insn_checker)
{
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};

	index &= 0xf; // QDSP-38444
	if ((index == 5) || (index == 4) || (index == 2) || (index == 3)) {
		// DM2,DM4,DM5 are global.
		// Hack unitl proper rewrite, simple write the same value to all
		for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
            dma_t *dma_ptr = thread->processor_ptr->dma[tnum];
			CALL_DMA_CMD(dma_cmd_cfgwr, dma_ptr, index, val, &report);
		}
	} else {
    	CALL_DMA_CMD(dma_cmd_cfgwr, dma, index, val, &report);
	}


	// We should relay the instruction completeness checker.
	if (insn_checker)
		(*insn_checker) = report.insn_checker;

	warn("DMA %d: ADAPTER  dma_cfgwr for dm%d thread=%u val=%x", dma->num, index, thread->threadId, val);
    return val;
}
void dma_adapter_set_dm5(dma_t *dma, uint32_t addr) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
    g_assert(thread->processor_ptr->runnable_threads_max > 0);
	for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
		dma_t *dma_ptr = thread->processor_ptr->dma[thread->threadId];
		CALL_DMA_CMD(dma_cmd_cfgwr, dma_ptr, 5, addr, NULL);
	}
}


size4u_t dma_adapter_cmd(thread_t *thread, dma_cmd_t opcode,
                         size4u_t arg1, size4u_t arg2) {
#if 0
	hex_exception_info einfo = {0};     // Storage to retrieve an exception if it occurs.
	/* Check if DMA is present. If there is none, just do a noop */
	if(!thread->processor_ptr->arch_proc_options->QDSP6_DMA_PRESENT) {
		if(!sys_in_monitor_mode(thread)) {
			decode_error(thread, &einfo, PRECISE_CAUSE_INVALID_PACKET);
			register_einfo(thread,&einfo);
		}
		return 0;
	}
#endif
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	size4u_t ret_val = 0;

	if (opcode < DMA_CMD_UND) {

		// Call an instruction implementation at the adapter side.warn("DMA %d: ADAPTER  dma_adapter_set_staller. tick count=%d", dma->num, dma_get_tick_count(dma));
		dma_adapter_cmd_impl_t impl = dma_adapter_cmd_impl_tab[opcode];

		if (impl != NULL) {
			// If the engine wants to stall, it will give us an instruction
			// completeness checker back.
			dma_insn_checker_ptr insn_checker = NULL;

	        assert(thread->processor_ptr->dma[thread->threadId]);
			ret_val = (*impl)(thread, arg1, arg2, &insn_checker);

			// If the engine gave us an instruction completeness checker, let's set up
			// a staller.
      		if (insn_checker != NULL) {
        		dma_adapter_set_staller(dma, insn_checker);
      		}
		}
	}

	return ret_val;
}

size4u_t dma_adapter_cmd_syncht(thread_t *thread, size4u_t dummy1, size4u_t dummy2,
                                dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};

	CALL_DMA_CMD(dma_cmd_syncht, dma, dummy1, dummy2, &report);

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	return 0;
}

size4u_t dma_adapter_cmd_tlbsynch(thread_t *thread, size4u_t dummy1, size4u_t dummy2,
                                dma_insn_checker_ptr *insn_checker) {
	// Obtain a current DMA instance from a thread ID.
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_cmd_report_t report = {.excpt = 0, .insn_checker = NULL};
	CALL_DMA_CMD(dma_cmd_tlbsynch, dma, dummy1, dummy2, &report);

	// We should relay the instruction completeness checker.
	(*insn_checker) = report.insn_checker;

	return 0;
}

void dma_adapter_pmu_increment(dma_t *dma, int event, int increment) {
	if (increment && (event < DMA_PMU_NUM)) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	INC_TSTATN(pmu_event_mapping[event], increment);
	if ((event == 0) && (thread->timing_on)){
		//dma_adapter_active_uarch_callback(dma, 0);
		}
	}
}
int dma_adapter_get_insn_latency(dma_t *dma, int index) {
	return 0;
}

void dma_adapter_active_uarch_callback(dma_t *dma, int lvl) {
}

dma_t * dma_adapter_get_dma(processor_t * proc, int dnum) {
	return proc->dma[dnum];
}

int dma_adapter_uwbc_ap_en(dma_t *dma) {
    return 0;
}
