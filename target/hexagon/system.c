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


//#include "thread.h"
//#include "arch.h"
#ifndef CONFIG_USER_ONLY
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "internal.h"
#include "hex_mmu.h"
#endif
#include <assert.h>
#include "macros.h"
#include "mmvec/mmvec.h"
#include "system.h"
#include "arch_options_calc.h"
//#include "external_api.h"
//#include "iic.h"
//#include "uarch/uarch.h"

//#define thread_t CPUHexagonState


#ifdef ZEBU_CHECKSUM_TRACE
#include "zebu_external_api.h"
#include "zebu_exec.h"
#endif


#include "string.h"
//#include "memwrap.h"

//#include "pmu.h"
//#include "isdb.h"

//#include "walk/walk.h"

//#include "clade_if.h"
//#include "clade2_if.h"

#define TLBGUESSIDX(VA) ( ((VA>>12)^(VA>>22)) & (MAX_TLB_GUESS_ENTRIES-1))

extern const size1u_t insn_allowed_uslot[][4];
extern const size1u_t insn_sitype[];
extern const size1u_t sitype_allowed_uslot[][4];
extern const char* sitype_name[];

void iic_flush_cache(processor_t * proc)

{
}

int hex_get_page_size(thread_t *thread, size4u_t vaddr, int width)

{
    int size = 1024 * 1024;
#ifndef CONFIG_USER_ONLY
    hwaddr phys;
    int prot;
    int32_t excp;
    /* make sure vaddr in tlb */
    hexagon_touch_memory(thread, vaddr, width);
    /* now get tlb size for this vaddr */
    if (!hex_tlb_find_match(thread, vaddr, MMU_DATA_LOAD, &phys, &prot, &size,
                            &excp, cpu_mmu_index(thread, false))) {
        HEX_DEBUG_LOG("%s: tlb lookup failed: vaddr=0x%x\n",
            __func__, vaddr);
        g_assert_not_reached();
    }
#endif
    return size;
}

paddr_t
mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
				enum mem_access_types mtype, int type_for_xlate)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];


#ifdef FIXME
	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	maptr->dropped_z = 0;

        hex_exception_info einfo;
#endif

	/* The basic stuff */
#ifdef FIXME
	maptr->bad_vaddr = maptr->vaddr = vaddr;
#else
	maptr->vaddr = vaddr;
#endif
	maptr->width = width;
	maptr->type = mtype;
#ifdef FIXME
	maptr->tnum = thread->threadId;
#endif
    maptr->cancelled = 0;
    maptr->valid = 1;

    int page_size = hex_get_page_size(thread, vaddr, width);
    maptr->size = 31 - clz32(page_size);

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
    maptr->paddr = vaddr;
    xlate_info_t *xinfo = &(maptr->xlate_info);
    memset(xinfo,0,sizeof(*xinfo));
    xinfo->size = maptr->size;

	return (maptr->paddr);
}

paddr_t
mem_init_access_unaligned(thread_t *thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
	enum mem_access_types mtype, int type_for_xlate)
{
	paddr_t ret;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	ret = mem_init_access(thread,slot,vaddr,1,mtype,type_for_xlate);
	maptr->vaddr = realvaddr;
	maptr->paddr -= (vaddr-realvaddr);
	maptr->width = size;
	return ret;
}

int sys_xlate_dma(thread_t *thread, size8u_t va, int access_type,
                  int maptr_type, int slot, size4u_t align_mask,
                  xlate_info_t *xinfo, hex_exception_info *einfo,
                  int extended_va, int vtcm_invalid, int dlbc, int forget)
{
  int ret = 1;

  memset(einfo,0,sizeof(*einfo));
  memset(xinfo,0,sizeof(*xinfo));
  xinfo->pte_u = 1;
  xinfo->pte_w = 1;
  xinfo->pte_r = 1;
  xinfo->pte_x = 1;
  xinfo->pa = (uint64_t)va;

  return ret;
}


#define FATAL_REPLAY
void
mem_dmalink_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, int slot)
{
	FATAL_REPLAY;

	mem_access_info_t *maptr = &thread->mem_access[slot];


	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	//maptr->dropped_z = 0;

        // hex_exception_info einfo = {0};

        /* The basic stuff */
	maptr->bad_vaddr = maptr->vaddr = vaddr;
	maptr->width = width;
	maptr->type = access_type_store;
	maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;

	thread->mem_access[slot].stdata = data;

	LOG_MEM_STORE(vaddr,maptr->paddr, width, data, slot);

}

#define warn(...)
#define env thread
#define Regs gpr
#define REG_PC HEX_REG_PC
#define EXCEPT_TYPE_PRECISE                 HEX_EVENT_PRECISE
#define EXCEPT_TYPE_TLB_MISS_RW             HEX_EVENT_TLB_MISS_RW
#define PRECISE_CAUSE_BIU_PRECISE           HEX_CAUSE_BIU_PRECISE
#define PRECISE_CAUSE_REG_WRITE_CONFLICT    HEX_CAUSE_REG_WRITE_CONFLICT
#define PRECISE_CAUSE_DOUBLE_EXCEPT         HEX_CAUSE_DOUBLE_EXCEPT

#ifndef CONFIG_USER_ONLY
static
bool is_du_badva_affecting_exception(int type, int cause)
{
	if (type == EXCEPT_TYPE_TLB_MISS_RW) {
		return true;
	}
	if ((type == EXCEPT_TYPE_PRECISE) && (cause >= 0x20)
		&& (cause <= 0x28)) {
		return true;
	}
	return false;
}

static
void
raise_coproc_ldst_exception(thread_t *env, size4u_t de_slotmask,
                            target_ulong PC)

{
    CPUState *cs = env_cpu(env);
    size4u_t slot = (de_slotmask & 0x1) ? 0 : 1;
    raise_perm_exception(cs, thread->einfo.badva1, slot, MMU_DATA_LOAD, thread->einfo.type);
    env->cause_code = thread->einfo.cause;
    do_raise_exception(env, cs->exception_index, PC, 0);
}

static
void
register_exception_info(thread_t * thread, size4u_t type, size4u_t cause,
						size4u_t badva0, size4u_t badva1, size4u_t bvs,
						size4u_t bv0, size4u_t bv1, size4u_t elr,
						size4u_t diag, size4u_t de_slotmask)
{
#ifdef VERIFICATION
	warn("Oldtype=%d oldcause=0x%x newtype=%d newcause=%x de_slotmask=%x diag=%x", thread->einfo.type, thread->einfo.cause, type, cause, de_slotmask, diag);
#endif
	if ((EXCEPTION_DETECTED)
		&& (thread->einfo.type == EXCEPT_TYPE_TLB_MISS_RW)
		&& ((type == EXCEPT_TYPE_PRECISE)
			&& ((cause == 0x28) || (cause == 0x29)))) {
		warn("Footnote in v2 System Architecture Spec 5.1 says: TLB miss RW has higher priority than multi write / bad cacheability");
	}

	else if ((EXCEPTION_DETECTED) && (thread->einfo.cause == PRECISE_CAUSE_BIU_PRECISE) && (cause == PRECISE_CAUSE_REG_WRITE_CONFLICT)) {
		warn("RTL Takes Multi-write before BIU, overwriting BIU");
		thread->einfo.type = type;
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
	} else if ((EXCEPTION_DETECTED) && (thread->einfo.bv1 && bv0)  &&
			/*We've already seen a slot1 exception */
			   is_du_badva_affecting_exception(thread->einfo.type,
							   thread->einfo.cause)
			   && is_du_badva_affecting_exception(type, cause)) {

		/* We've already seen a slot1 D-side exception, so only
		   need to record the BADVA0 info */
		thread->einfo.badva0 = badva0;
		thread->einfo.bv0 = bv0;
	} else if ((!EXCEPTION_DETECTED) || (type < thread->einfo.type)) {
		SET_EXCEPTION;
		thread->einfo.type = type;
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
		thread->einfo.de_slotmask |= de_slotmask;
        raise_coproc_ldst_exception(thread, de_slotmask, elr);
	} else if ((type == thread->einfo.type)
			   && (cause < thread->einfo.cause)) {
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
	} else if ((type == thread->einfo.type)
			   && (cause == thread->einfo.cause)
			   && (cause == PRECISE_CAUSE_DOUBLE_EXCEPT)) {
		if ((de_slotmask == 0)
			|| (thread->einfo.de_slotmask < de_slotmask)) {
			if (diag < thread->einfo.diag) {
				warn("Picking better proiroty root exception cause for DIAG: 0x%x", diag);
				thread->einfo.diag = diag;
			} else {
				warn("Not selecting lower priority DIAG: 0x%x", diag);
			}
			thread->einfo.de_slotmask |= de_slotmask;
		} else {
			warn("Trying to avoid slot0 DE in the presence of a slot1 DE, not setting slot0 DIAG of 0x%x", diag);
		}
	} else {
		/* do nothing, lower prio */
	}
}

static
void
register_error_exception(thread_t * thread, size4u_t error_code,
						 size4u_t badva0, size4u_t badva1, size4u_t bvs,
						 size4u_t bv0, size4u_t bv1, size4u_t slotmask)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
	/*warn("Error exception detected, tnum=%d code=0x%x pc=0x%x badva0=0x%x badva1=0x%x, bvs=%x, Pcycle=%lld msg=%s\n", thread->threadId, error_code, thread->Regs[REG_PC], badva0, badva1, bvs, thread->processor_ptr->monotonic_pcycles, thread->exception_msg ? thread->exception_msg : "");*/
	/*thread->exception_msg = NULL;*/
	if ((error_code > HEX_CAUSE_DOUBLE_EXCEPT)
		&& GET_SSR_FIELD(SSR_EX, ssr)) {
		/*warn("Double Exception...");*/
		register_exception_info(thread, EXCEPT_TYPE_PRECISE,
								HEX_CAUSE_DOUBLE_EXCEPT,
								ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA0),
								ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA1),
								GET_SSR_FIELD(SSR_BVS, ssr),
								GET_SSR_FIELD(SSR_V0, ssr),
								GET_SSR_FIELD(SSR_V1, ssr),
								thread->Regs[REG_PC], error_code,
								slotmask);
		return;
	}

	register_exception_info(thread, EXCEPT_TYPE_PRECISE, error_code,
							badva0, badva1, bvs, bv0, bv1,
							thread->Regs[REG_PC],
							ARCH_GET_SYSTEM_REG(thread, HEX_SREG_DIAG),
							0);
}
#endif

void register_coproc_ldst_exception(thread_t * thread, int slot, size4u_t badva)
{
#ifndef CONFIG_USER_ONLY
	/*warn("Coprocessor LDST Exception, tnum=%d npc=%x\n", thread->threadId, thread->Regs[REG_PC]);*/
	if (slot == 0) {
		register_error_exception(thread, HEX_CAUSE_COPROC_LDST,
			 badva,
			 ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA),
			 0 /* select 0 */,
			 1 /* set bv0 */,
			 0 /* clear bv1 */, 1<<slot);
	} else {
		register_error_exception(thread, HEX_CAUSE_COPROC_LDST,
			ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA0),
			badva,
			1 /* select 1 */,
			0 /* clear bv0 */,
			1 /* set bv1 */, 1<<slot);
	}
#else
    printf("ERROR: register_coproc_ldst_exception not implemented\n");
    g_assert_not_reached();
#endif
}

