/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef CONFIG_USER_ONLY
#define thread_t CPUHexagonState
#endif
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "opcodes.h"
#include "insn.h"
#include "arch_options_calc.h"
#include "system.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif
#include "target/hexagon/internal.h"

#include "qemu/osdep.h"
#include "cpu.h"
#define THREAD2STRUCT thread
#include "mmvec/system_ext_mmvec.h"

typedef Insn insn_t;

void mem_gather_store(CPUHexagonState *env, target_ulong vaddr, int slot)
{
    size_t size = VECTOR_SIZE_BYTE;

    env->vstore_pending[slot] = 1;
    env->vstore[slot].va   = vaddr;
    env->vstore[slot].size = size;
    memcpy(&env->vstore[slot].data.ub[0], &env->tmp_VRegs[0], size);

    /* On a gather store, overwrite the store mask to emulate dropped gathers */
    bitmap_copy(env->vstore[slot].mask, env->vtcm_log.mask, size);
}

#ifndef CONFIG_USER_ONLY
#define FATAL_REPLAY
#ifdef EXCEPTION_DETECTED
#undef EXCEPTION_DETECTED
#define EXCEPTION_DETECTED      (thread->status & EXEC_STATUS_EXCEPTION)
#endif
#define warn(...) {}
//#define register_coproc_ldst_exception(...) {}

#define TYPE_LOAD 'L'
#define TYPE_STORE 'S'
#define TYPE_FETCH 'F'
#define TYPE_ICINVA 'I'


// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)

static int check_scatter_gather_page_cross(thread_t* thread, vaddr_t base, int length, int page_size)
{

    return 0;
}
#endif

void mem_vector_scatter_init(thread_t* thread, insn_t * insn, vaddr_t base_vaddr, int length, int element_size)
{
#ifndef CONFIG_USER_ONLY
    enum ext_mem_access_types access_type=access_type_vscatter_store;
    // Translation for Store Address on Slot 1 - maybe any slot?
    int slot = insn->slot;
    mem_access_info_t * maptr = &thread->mem_access[slot];
    maptr->use_aligned_address = 0;
    maptr->is_coproc_range = 1;
    mem_init_access_unaligned(thread, slot, base_vaddr, base_vaddr, length, (enum mem_access_types) access_type, TYPE_STORE);
    mmvecx_t *mmvecx = THREAD2STRUCT;

    // Check TCM space for Exception
    paddr_t base_paddr = thread->mem_access[slot].paddr;

    thread->mem_access[slot].paddr = thread->mem_access[slot].paddr & ~(element_size-1);   // Align to element Size

	int in_tcm = in_vtcm_space(thread,base_paddr,SHOW_WARNING);
    if (maptr->xlate_info.memtype.device && !in_tcm) register_coproc_ldst_exception(thread,slot,base_vaddr);

    int scatter_gather_exception =  (length < 0);
    scatter_gather_exception |= !in_tcm;
    scatter_gather_exception |= !in_vtcm_space(thread,base_paddr+length, SHOW_WARNING);
    scatter_gather_exception |= check_scatter_gather_page_cross(thread,base_vaddr,length, thread->mem_access[slot].xlate_info.size);
    if (scatter_gather_exception)
        register_coproc_ldst_exception(thread,slot,base_vaddr);

    if (EXCEPTION_DETECTED) return;


	maptr->range = length;

    int i = 0;
    for(i = 0; i < fVECSIZE(); i++) {
        fVARRAY_ELEMENT_ACCESS(mmvecx->vtcm_log.offsets, ub, i) = 0; // Mark invalid
        mmvecx->vtcm_log.data.ub[i] = 0;
        //mmvecx->vtcm_log.mask.ub[i] = 0;
        mmvecx->vtcm_log.pa[i] = 0;
    }
    mmvecx->vtcm_log.va_base = base_vaddr;
    mmvecx->vtcm_log.pa_base = base_paddr;

    mmvecx->vtcm_log.oob_access = 0;
#endif
    thread->vtcm_pending = true;
    thread->vtcm_log.op = false;
    thread->vtcm_log.op_size = 0;
    bitmap_zero(thread->vtcm_log.mask, MAX_VEC_SIZE_BYTES);
}


void mem_vector_gather_init(thread_t* thread, Insn * insn, vaddr_t base_vaddr,  int length, int element_size)
{
#ifndef CONFIG_USER_ONLY
    int slot = insn->slot;
    enum ext_mem_access_types access_type = access_type_vgather_load;
    mem_access_info_t * maptr = &thread->mem_access[slot];
    maptr->use_aligned_address = 0;
    maptr->is_coproc_range = 1;
    mem_init_access_unaligned(thread, slot, base_vaddr, base_vaddr, length, (enum mem_access_types) access_type, TYPE_LOAD);
    mmvecx_t *mmvecx = THREAD2STRUCT;

    // Check TCM space for Exception
    paddr_t base_paddr = thread->mem_access[slot].paddr;



    thread->mem_access[slot].paddr = thread->mem_access[slot].paddr & ~(element_size-1);   // Align to element Size

    // Need to Test 4 conditions for exception
    // M register is positive
    // Base and Base+Length-1 are in TCM
    // Base + Length doesn't cross a page
	int in_tcm = in_vtcm_space(thread,base_paddr, SHOW_WARNING);
    if (maptr->xlate_info.memtype.device && !in_tcm) register_coproc_ldst_exception(thread,slot,base_vaddr);

    int scatter_gather_exception =  (length < 0);
    scatter_gather_exception |= !in_tcm;
    scatter_gather_exception |= !in_vtcm_space(thread,base_paddr+length, SHOW_WARNING);
    scatter_gather_exception |= check_scatter_gather_page_cross(thread,base_vaddr,length, thread->mem_access[slot].xlate_info.size);
    if (scatter_gather_exception)
        register_coproc_ldst_exception(thread,slot,base_vaddr);

    if (EXCEPTION_DETECTED) return;

	maptr->range = length;

    int i = 0;
    for(i = 0; i < 2*fVECSIZE(); i++) {
        fVARRAY_ELEMENT_ACCESS(mmvecx->vtcm_log.offsets, ub, i) = 0x0;
    }
    for(i = 0; i < fVECSIZE(); i++) {
        mmvecx->vtcm_log.data.ub[i] = 0;
        //mmvecx->vtcm_log.mask.ub[i] = 0;
        mmvecx->vtcm_log.pa[i] = 0;
        mmvecx->tmp_VRegs[0].ub[i] = 0;
    }
    mmvecx->vtcm_log.oob_access = 0;
    mmvecx->vtcm_log.op = 0;
    mmvecx->vtcm_log.op_size = 0;

    mmvecx->vtcm_log.va_base = base_vaddr;
    mmvecx->vtcm_log.pa_base = base_paddr;

    // Temp Reg gets updated
    // This allows Store .new to grab the correct result
    mmvecx->VRegs_updated_tmp = 0xFFFFFFFF;
    mmvecx->gather_issued = 1;
#endif    
    bitmap_zero(thread->vtcm_log.mask, MAX_VEC_SIZE_BYTES);
}


#ifndef CONFIG_USER_ONLY
void mem_vector_scatter_finish(thread_t* thread, insn_t * insn, int op)
{

    int slot = insn->slot;
    mmvecx_t *mmvecx = THREAD2STRUCT;
#if 0
    for (i = 0; i < fVECSIZE(); i++)
    {
        if ( mmvecx->vtcm_log.mask.ub[i] )
        {
            if (ARCH_OPT_TH(vtcm_poison_enable) && !thread->bq_on)
            {
              	enlist_byte(thread, mmvecx->vtcm_log.pa[i], POISON, 2); //rw=2
            }
        }
    }
#endif
    thread->vstore_pending[slot] = 0;
    mmvecx->vstore_pending[slot] = 0;
    mmvecx->vtcm_log.size = fVECSIZE();


    memcpy(thread->mem_access[slot].cdata, &mmvecx->vtcm_log.offsets.v[0].ub[0], 128);
    memcpy(&thread->mem_access[slot].cdata[128], &mmvecx->vtcm_log.offsets.v[1].ub[0], 128);




    return;
}

void mem_vector_gather_finish(thread_t* thread, insn_t * insn)
{
    // Gather Loads
    int slot = insn->slot;
    mmvecx_t *mmvecx = THREAD2STRUCT;

#if 0
    // FIXME: Move this to ext.c at commit time
    for (i = 0; i < fVECSIZE(); i++)
    {
        if ( mmvecx->vtcm_log.mask.ub[i] )
        {
            if (ARCH_OPT_TH(vtcm_poison_enable) && !thread->bq_on)
            {
              //                read_vtcm_poison_scatter_gather_byte(thread, mmvecx->vtcm_log.pa[i]);
                enlist_byte(thread, mmvecx->vtcm_log.pa[i], POISON, 1); //rw=1, read-poison
            }
        }
    }
#endif
    memcpy(thread->mem_access[slot].cdata, &mmvecx->vtcm_log.offsets.v[0].ub[0], 128);
    memcpy(&thread->mem_access[slot].cdata[128], &mmvecx->vtcm_log.offsets.v[1].ub[0], 128);


}
#endif

bool is_daz_mode(thread_t *thread) {
	return GET_FIELD(USR_FPDAZ, thread->gpr[HEX_REG_USR]);
}

uint32_t get_usr_reg_fpsat_field(thread_t* thread) {
	return GET_FIELD(USR_FPSAT, thread->gpr[HEX_REG_USR]);
}
