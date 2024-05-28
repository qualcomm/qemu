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

#define ARCHOPT(OPTION)  (proc->arch_proc_options->OPTION)

#ifdef VERIFICATION
#include "ver_external_api.h"
#include "ver_exec.h"
#endif

#ifdef EMU_CHECKSUM_TRACE
#include "emu_external_api.h"
#include "emu_exec.h"
#endif


#include "string.h"
//#include "memwrap.h"

//#include "pmu.h"
//#include "isdb.h"

//#include "walk/walk.h"

//#include "clade_if.h"
//#include "clade2_if.h"
//#include "mmvec/mmvec.h"
//#include "cacheability_auto.h"

//#include "arch_options_calc.h"


//#include "q6v_system.c"


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
    hexagon_touch_memory(thread, vaddr, width, HEX_MEM_READ);
    /* now get tlb size for this vaddr */
    if (!hex_tlb_find_match(thread, vaddr, MMU_DATA_LOAD, &phys, &prot, &size,
                            &excp, cpu_mmu_index(env_cpu(thread), false))) {
        HEX_DEBUG_LOG("%s: tlb lookup failed: vaddr=0x%x\n",
            __func__, vaddr);
        g_assert_not_reached();
    }
#endif
    return size;
}

#define SYSVERWARN(...)
#define warn(...)
#define env thread
#define Regs gpr
#define REG_PC HEX_REG_PC
#define REG_BADVA0 HEX_SREG_BADVA0
#define REG_BADVA1 HEX_SREG_BADVA1
#define EXCEPT_TYPE_PRECISE                 HEX_EVENT_PRECISE
#define EXCEPT_TYPE_TLB_MISS_RW             HEX_EVENT_TLB_MISS_RW
#define PRECISE_CAUSE_BIU_PRECISE           HEX_CAUSE_BIU_PRECISE
#define PRECISE_CAUSE_REG_WRITE_CONFLICT    HEX_CAUSE_REG_WRITE_CONFLICT
#define PRECISE_CAUSE_DOUBLE_EXCEPT         HEX_CAUSE_DOUBLE_EXCEPT

void
register_exception_info(thread_t * thread, size4u_t type, size4u_t cause,
						size4u_t badva0, size4u_t badva1, size4u_t bvs,
						size4u_t bv0, size4u_t bv1, size4u_t elr,
						size4u_t diag, size4u_t de_slotmask);


#ifndef CONFIG_USER_ONLY
/* Function to check if a VTCM access is falling into  a VTCM window assigned. */
static int is_vtcm_acc_window_violation(thread_t *thread, paddr_t pa)
{
  int violation = 0;
  uint32_t vwctrl = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_VWCTRL);
  int enabled = GET_FIELD(VWCTRL_VWENABLE, vwctrl) & 0x1;
  paddr_t win_lo_addr = 0;
  paddr_t win_hi_addr = 0;
  if (enabled == 0) {
    violation = 1;
  } else {
    win_lo_addr = get_vtcm_base(thread) + (GET_FIELD(VWCTRL_LOWOFFSET, vwctrl) << 12);
    win_hi_addr = get_vtcm_base(thread) + ((GET_FIELD(VWCTRL_HIOFFSET, vwctrl) << 12) | 0xfff);
    violation = (pa < win_lo_addr || pa > win_hi_addr);
  }
  if (violation){
    SYSVERWARN("VTCM access window violation: violation=%d enabled=%d paddr=0x%lx vwctrl_low=0x%lx vwctrl_hi=0x%lx lower:%d higher:%d", violation, enabled, pa, win_lo_addr, win_hi_addr, (pa < win_lo_addr), (pa > win_hi_addr));
  }
  return violation;
}

static void register_einfo(thread_t *thread, hex_exception_info *einfo)
{
        target_ulong ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
        int register_double_exception = (GET_SSR_FIELD(SSR_EX, ssr)>0);
        warn ("register_einfo  cause: %x\n", einfo->cause);
        // Imprecise can't cause double exception, but TB doesn't check anything on imprecise exception
        // Precise, but higher priority than double, can't cause a double
        if ((einfo->type == EXCEPT_TYPE_PRECISE) && (einfo->cause < PRECISE_CAUSE_DOUBLE_EXCEPT))
                register_double_exception = 0;

        if (register_double_exception) {
                warn("Double Exception (from: type=%x cause=%x elr=%x)",einfo->type,einfo->cause,einfo->elr);
                register_exception_info(thread,EXCEPT_TYPE_PRECISE,PRECISE_CAUSE_DOUBLE_EXCEPT,
                        thread->Regs[REG_BADVA0],thread->Regs[REG_BADVA1],
                        GET_SSR_FIELD(SSR_BVS, ssr), GET_SSR_FIELD(SSR_V0, ssr), GET_SSR_FIELD(SSR_V1, ssr),
                        einfo->elr, einfo->cause, einfo->de_slotmask);
        } else {
                warn("Registering exception info: type=%x cause=%x elr=%x badva0=%x badva1=%x bvs=%d",
                        einfo->type,einfo->cause,einfo->elr,einfo->badva0,einfo->badva1,einfo->bvs);
                register_exception_info(thread,einfo->type,einfo->cause,einfo->badva0,
                        einfo->badva1, einfo->bvs,einfo->bv0,einfo->bv1,einfo->elr,
                        ARCH_GET_SYSTEM_REG(thread, HEX_SREG_DIAG),0);
        }
}

static void fill_einfo_ldst(thread_t *thread, hex_exception_info *einfo, size4u_t type, size4u_t slot,
                size4u_t cause, size4u_t va)
{
        target_ulong ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
        memset(einfo,0,sizeof(*einfo));
        einfo->valid = 1;
        einfo->type = type;
        einfo->cause = cause;
        //DAG: If cause is BIU PRECISE, then SSR bits don't need an update so set them to their existing values
        if(cause == PRECISE_CAUSE_BIU_PRECISE) {
                einfo->badva0 = thread->Regs[REG_BADVA0];
                einfo->badva1 = thread->Regs[REG_BADVA1];
                einfo->bv0 = GET_SSR_FIELD(SSR_V0, ssr);
                einfo->bv1 = GET_SSR_FIELD(SSR_V1, ssr);
                einfo->bvs = GET_SSR_FIELD(SSR_BVS, ssr);
        }
        else if (slot == 0) {
                einfo->badva0 = va;
                einfo->badva1 = thread->Regs[REG_BADVA1];
                einfo->bv0 = 1;
                einfo->bv1 = 0;
                einfo->bvs = 0;
        } else {
                einfo->badva0 = thread->Regs[REG_BADVA0];
                einfo->badva1 = va;
                einfo->bv0 = 0;
                einfo->bv1 = 1;
                einfo->bvs = 1;
        }
        einfo->elr = thread->Regs[REG_PC];
        einfo->de_slotmask = 1<<slot;
}

static inline void fill_einfo_ldsterror(thread_t *thread, hex_exception_info *einfo, size4u_t slot,
                size4u_t cause, size4u_t va) {
        fill_einfo_ldst(thread,einfo,EXCEPT_TYPE_PRECISE,slot,cause,va);
}

static void sys_check_vwctrl(thread_t *thread, int slot, vaddr_t va, paddr_t pa)
{
  /* Here we will be checking for if the given pa lies in the vtcm window */
  if(!in_vtcm_space(thread,pa,HIDE_WARNING)) {
    SYSVERWARN("VTCM access window not in vtcm");
    return;
  }
  /*0x8fff0000 is low=0 hi=0xfff en=1, which means the full VTCM range is accessible*/
  if(ARCH_GET_SYSTEM_REG(thread, HEX_SREG_VWCTRL) == 0x8fff0000) {
    SYSVERWARN("VTCM access window default value set");
    return;
  }
  if(is_vtcm_acc_window_violation(thread,pa)) {
    /* QDSP-73523: Do not throw any exception in monitor mode */
    if (!(sys_in_monitor_mode(thread))) {
      hex_exception_info einfo;
      fill_einfo_ldsterror(thread,&einfo,slot,HEX_CAUSE_VWCTRL_WINDOW_MISS,va);
      register_einfo(thread,&einfo);
    }
  }
}
#endif

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

    /* This fn is called for different mem type insrns
        We need to make sure that the start address lies in vtcm range */
#ifndef CONFIG_USER_ONLY
    sys_check_vwctrl(thread, slot, maptr->vaddr, maptr->paddr);
#endif
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

#ifndef CONFIG_USER_ONLY
        paddr_t base = (paddr_t)maptr->paddr;
        if (maptr->use_aligned_address) {
          paddr_t mask = 0x7F;
          base = base & mask;
        }
        /* Below calculation makes sure we have the right value when
           we have negative size */
        paddr_t end;
        if (size > 0) {
          if (maptr->is_coproc_range) {
            end = (paddr_t)(((int64_t)base + size));
          }
          else {
            end = (paddr_t)(((int64_t)base + size - 1));
          }
        }
        else {
          end = (paddr_t)(((int64_t)base + size ));
        }
       sys_check_vwctrl(thread, slot, maptr->vaddr, end);
#endif
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
#if 0
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->xe = GET_SSR_FIELD(SSR_XE);
    maptr->xa = GET_SSR_FIELD(SSR_XA);

       /* For trace in the uarch */
       maptr->pc_va = thread->Regs[REG_PC];


       // Different here, we're not going to take an exception on dmlink, but the dmwait
       // if this packet has an exception, don't log the store
       if(sys_xlate_dma(thread,vaddr,TYPE_STORE,access_type_store, slot, width-1, &maptr->xlate_info, &einfo)==0) {
               SYSVERWARN("not doing dmlink store due to potential exception");
               if (!thread->processor_ptr->options->testgen_mode) {
                       MEMTRACE_ST(thread, thread->Regs[REG_PC], vaddr, 0, width, DWRITE, data);
               }
               return;
       }

       maptr->paddr = maptr->xlate_info.pa;
#else
  maptr->paddr = vaddr;
#endif

       thread->mem_access[slot].stdata = data;

       LOG_MEM_STORE(vaddr,maptr->paddr, width, data, slot);

}

#ifndef CONFIG_USER_ONLY
static
int is_du_badva_affecting_exception(int type, int cause)
{
	if (type == EXCEPT_TYPE_TLB_MISS_RW) {
		return 1;
	}
	if ((type == EXCEPT_TYPE_PRECISE) && (cause >= 0x20)
		&& (cause <= HEX_CAUSE_VWCTRL_WINDOW_MISS)) {
		return 1;
	}
	return (0);
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

void
register_exception_info(thread_t * thread, size4u_t type, size4u_t cause,
						size4u_t badva0, size4u_t badva1, size4u_t bvs,
						size4u_t bv0, size4u_t bv1, size4u_t elr,
						size4u_t diag, size4u_t de_slotmask)
{
#ifdef VERIFICATION
	warn("Oldtype=%d oldcause=0x%x newtype=%d newcause=%x de_slotmask=%x diag=%x", thread->einfo.type, thread->einfo.cause, type, cause, de_slotmask, diag);
#endif
	warn("Oldtype=%d oldcause=0x%x newtype=%d newcause=%x de_slotmask=%x diag=%x", thread->einfo.type, thread->einfo.cause, type, cause, de_slotmask, diag);
        if ((EXCEPTION_DETECTED)
                 && (thread->einfo.type == 0x0B)) { // EXCEPT_TYPE_FPTRAP
                 warn("FP Exception has higher priority than multi write / bad cacheability");
       }
       else if ((EXCEPTION_DETECTED)
		&& (thread->einfo.type == EXCEPT_TYPE_TLB_MISS_RW)
		&& ((type == EXCEPT_TYPE_PRECISE)
			&& ((cause == 0x26) || (cause == 0x27) || (cause == 0x28) || (cause == 0x29)))) {
		warn("Footnote in v2 System Architecture Spec 5.1 says: TLB miss RW has higher priority than multi write / bad cacheability");
	} 
	
	else if ((EXCEPTION_DETECTED) && (thread->einfo.cause == PRECISE_CAUSE_BIU_PRECISE) && ((cause == PRECISE_CAUSE_REG_WRITE_CONFLICT)||(cause == PRECISE_CAUSE_DOUBLE_EXCEPT))) {
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

void
register_error_exception(thread_t * thread, size4u_t error_code,
						 size4u_t badva0, size4u_t badva1, size4u_t bvs,
						 size4u_t bv0, size4u_t bv1, size4u_t slotmask);
void
register_error_exception(thread_t * thread, size4u_t error_code,
						 size4u_t badva0, size4u_t badva1, size4u_t bvs,
						 size4u_t bv0, size4u_t bv1, size4u_t slotmask)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(thread, HEX_SREG_SSR);
	//warn("Error exception detected, tnum=%d code=0x%x pc=0x%x badva0=0x%x badva1=0x%x, bvs=%x, Pcycle=%lld msg=%s\n", thread->threadId, error_code, thread->Regs[REG_PC], badva0, badva1, bvs, thread->processor_ptr->monotonic_pcycles, thread->exception_msg ? thread->exception_msg : "");
	//thread->exception_msg = NULL;
	if ((error_code > PRECISE_CAUSE_DOUBLE_EXCEPT)
		&& GET_SSR_FIELD(SSR_EX, ssr)) {
		//warn("Double Exception...");
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
	//warn("Coprocessor LDST Exception, tnum=%d npc=%x\n", thread->threadId, thread->Regs[REG_PC]);
	if (slot == 0) {
		register_error_exception(thread, HEX_CAUSE_COPROC_LDST,
			 badva,
			 ARCH_GET_SYSTEM_REG(thread, HEX_SREG_BADVA),
			 0 /* select 0 */,
			 1 /* set bv0 */,
			 0 /* clear bv1 */, 1<<slot);
	} else {
		register_error_exception(thread,HEX_CAUSE_COPROC_LDST,
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
