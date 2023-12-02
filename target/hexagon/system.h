/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef _SYSTEM_H
#define _SYSTEM_H

#undef LOG_MEM_STORE
#define LOG_MEM_STORE(...)

/* FIXME: Use the mem_access_types instead */
#define TYPE_LOAD 'L'
#define TYPE_STORE 'S'
#define TYPE_FETCH 'F'
#define TYPE_ICINVA 'I'
#define TYPE_DMA_FETCH 'D'

//#include "thread.h"
#include "macros.h"
#include "sys_macros.h"
#include "xlate_info.h"
#include "iclass.h"
//#include "memwrap.h"

#define thread_t CPUHexagonState

#define TCM_UPPER_BOUND 0x400000
#define L2VIC_UPPER_BOUND 0x10000
#define TCM_RANGE(PROC, PADDR)                                  \
    (((PADDR) >= (PROC)->options->l2tcm_base) &&                \
     ((PADDR) < (PROC)->options->l2tcm_base + TCM_UPPER_BOUND))

#define L2VIC_RANGE(PROC, PADDR)                                  \
	((0 != get_fastl2vic_base((PROC))) &&											\
	 ((PADDR) >= get_fastl2vic_base((PROC))) &&								\
	 ((PADDR) < get_fastl2vic_base((PROC)) + L2VIC_UPPER_BOUND))


#define VERIFY_EXTENSIVELY(PROC, A)              \
    if(!((PROC)->options->dont_verify_extensively)) { A; }

#define CFG_TABLE_RANGE(PROC, PADDR)																	\
	(((PADDR) >= ((size8u_t)(PROC)->global_regs[REG_CFGBASE]) << 16) &&							\
	 ((PADDR) < (((size8u_t)(PROC)->global_regs[REG_CFGBASE]) << 16) + (0x1 << 16)))

#define CORE_SLOTMASK (thread->processor_ptr->arch_proc_options->QDSP6_TINY_CORE ? 13 : 15)

int hex_get_page_size(thread_t *thread, size4u_t vaddr, int width);
paddr_t mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
		enum mem_access_types mtype, int type_for_xlate);

paddr_t mem_init_access_unaligned(thread_t * thread, int slot, size4u_t vaddr,
        size4u_t realvaddr, int size, enum mem_access_types mtype,
        int type_for_xlate);

int sys_xlate_dma(thread_t *thread, size8u_t va, int access_type,
                  int maptr_type, int slot, size4u_t align_mask,
                  xlate_info_t *xinfo, hex_exception_info *einfo,
                  int extended_va, int vtcm_invalid, int dlbc, int forget);
void register_dma_error_exception(thread_t *thread, hex_exception_info *einfo,
                                  size4u_t va);
void iic_flush_cache(processor_t * proc);
void mem_dmalink_store(thread_t * thread, size4u_t vaddr, int width,
    size8u_t data, int slot);
void register_coproc_ldst_exception(thread_t * thread, int slot,
    size4u_t badva);
int check_coproc_page_cross(thread_t* thread, vaddr_t base,
    int length, int page_size);
#endif
