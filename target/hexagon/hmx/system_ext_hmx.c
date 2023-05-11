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

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "system.h"
#include "opcodes.h"
#include "insn.h"
#include "hmx/system_ext_hmx.h"
#include "arch_options_calc.h"
#include "hmx/macros_auto.h"
#include "hmx/hmx_int_ops.h"

#define env thread
#define thread_t         CPUHexagonState
#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)
#define Regs gpr
#define REG_USR HEX_REG_USR

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)
#define MEMTRACE_LD(...)
#define memwrap_memtrace_ld(...)
#define memwrap_memtrace_st(...)
#define warn(...) {}

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)

static int check_mxmem_page_cross(thread_t* thread, vaddr_t base,
    int length, int page_size)
{
#ifndef CONFIG_USER_ONLY
	vaddr_t page_mask = (1ULL<<page_size)-1;
	if (((base+length) & ~page_mask) != (base & ~page_mask)) {
		warn("HMX Op crossed page: start =%x end=%x Page Size=%x Page Mask=%x",
            base, base+length, page_size,page_mask);
		return 1;
	}
#endif
    return 0;
}

#define MX_EXCEPTION_CHECK(VA,LEN,LEN2,TYPE1, TYPE2)\
	mem_init_access_unaligned(thread, slot, VA, VA, LEN, TYPE1, TYPE2);\
    if (EXCEPTION_DETECTED) return;\
	paddr = maptr->paddr;\
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;\
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);\
    CHECK_ACCESS_RANGE(mxmem_exception, paddr, LEN);\
    mxmem_exception |= check_mxmem_page_cross(thread,VA,LEN2, thread->mem_access[slot].xlate_info.size);\
    if (mxmem_exception) {\
	        register_coproc_ldst_exception(thread,slot,VA);\
	}\
	if (EXCEPTION_DETECTED) return;



void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size)
{
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	int type = (access_type == access_type_hmx_load_bias) ? TYPE_LOAD : TYPE_STORE;

	if(size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 8)
	{
		hmx_maptr->bias_32bit = 1;
		//temprarily use bias load as bias load2 for Netrani before the correct modeling
		if(thread->processor_ptr->arch_proc_options->QDSP6_MX_RATE == 1)
			hmx_maptr->bias_32bit = 0;
	}
	else if (size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 4)
		hmx_maptr->bias_32bit = 0;
	else
		warn(":unknown bias data type, neither 16bit nor 32bit ");

	mem_init_access_unaligned(thread, slot, vaddr, vaddr, size, (enum mem_access_types) access_type, type);\

    if (EXCEPTION_DETECTED) return;
	maptr->paddr = paddr = maptr->paddr & ~(size-1);

	maptr->width = size;

	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
	CHECK_ACCESS_RANGE(mxmem_exception, paddr, size-1);
    if (mxmem_exception) {
        register_coproc_ldst_exception(thread,slot,vaddr);
	}
	if (EXCEPTION_DETECTED) return;

#ifdef VERIFICATION
    int slot_tmp = thread->cur_slot;
    thread->cur_slot = slot;
#endif
	if(!thread->bq_on) {
		if (access_type == access_type_hmx_load_bias) {
			memwrap_memtrace_ld(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		} else {
			memwrap_memtrace_st(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		}
	}
#ifdef VERIFICATION
    thread->cur_slot = slot_tmp;
#endif

}

const char * hmx_act_type_str[] = {"BLOCK", "DEEP", "ABOVE", "SINGLE", "DILATE", "BATCH"};
const char * hmx_wgt_type_str[] = {"NORMAL", "DEEP", "AFTER", "SINGLE", "DILATE", "DROP"};


void hmx_debug_log_mac_info(thread_t * thread) {
#if 0
	mem_access_info_t *wgt_maptr = &thread->mem_access[0];
	mem_access_info_t *act_maptr = &thread->mem_access[1];
	hmx_mem_access_info_t * act_hmx_maptr = &act_maptr->hmx_ma;

	fMX_DEBUG_LOG(0, "MXMEM %s MAC OPERATION pktid=%08x", (THREAD2HMXSTRUCT->is_flt) ? "FLT" : "FXP", thread->pktid);
	fMX_DEBUG_LOG(0, "\tACT start=%08x range=%08x pa=%010llx operation type=%s", act_maptr->vaddr, act_maptr->range, act_maptr->paddr, hmx_act_type_str[THREAD2HMXSTRUCT->act_block_type]);
	fMX_DEBUG_LOG(0, "\tWGT start=%08x range=%08x pa=%010llx operation type=%s", wgt_maptr->vaddr, wgt_maptr->range, wgt_maptr->paddr, hmx_wgt_type_str[THREAD2HMXSTRUCT->wgt_block_type] );
	fMX_DEBUG_LOG(0, "\tCrouton: x_mask: 0x%02x x_inc: 0x%02x y_mask: 0x%02x y_inc: 0x%02x", THREAD2HMXSTRUCT->tile_x_mask, THREAD2HMXSTRUCT->tile_x_inc, THREAD2HMXSTRUCT->tile_y_mask, THREAD2HMXSTRUCT->tile_y_inc);
	fMX_DEBUG_LOG(0, "\tACT dY: %08x Croutons=%2d fx: %2d fy: %2d (%2d x %2d) y_start=%2d y_stop=%2d ch_start=%2d ch_stop=%2d group size=%2d", THREAD2HMXSTRUCT->dY, THREAD2HMXSTRUCT->blocks, THREAD2HMXSTRUCT->fx, THREAD2HMXSTRUCT->fy, THREAD2HMXSTRUCT->x_tap, THREAD2HMXSTRUCT->y_tap, THREAD2HMXSTRUCT->y_start, THREAD2HMXSTRUCT->y_stop, THREAD2HMXSTRUCT->ch_start, THREAD2HMXSTRUCT->ch_stop, THREAD2HMXSTRUCT->group_size );
	fMX_DEBUG_LOG(0, "\tWGT maximum pa=%010llx decompression mode: %d start offset: %x, global density: %x ",  THREAD2HMXSTRUCT->max_weight_pa, THREAD2HMXSTRUCT->wgtc_mode, THREAD2HMXSTRUCT->wgtc_start_offset, THREAD2HMXSTRUCT->wgtc_total_bytes);
	if(act_hmx_maptr->flt) {
		fMX_DEBUG_LOG(0, "\tHMX IEEE FP USR INF/NAN: %x Negate WGT: %x BF16:%d ",  THREAD2HMXSTRUCT->usr_fp.raw, THREAD2HMXSTRUCT->wgt_negate, THREAD2HMXSTRUCT->is_bf16);
	}
#endif
}



void hmx_act_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot)
{
    paddr_t paddr;
    mem_access_info_t *maptr = &thread->mem_access[slot];
	paddr_t pa_mask = 2047;
	pa_mask = ~pa_mask;
	vaddr_t addr_mask = ~2047;
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	int dY = range & addr_mask;
	MX_EXCEPTION_CHECK(start, dY, dY, access_type_hmx_load_act, TYPE_LOAD);
	maptr->vaddr = start;
	memset(hmx_maptr, 0, sizeof(hmx_mem_access_info_t));
	maptr->range = range;
	maptr->width = 2048;
	maptr->paddr = paddr & pa_mask;
	THREAD2HMXSTRUCT->act_addr = maptr->paddr;
#ifdef VERIFICATION
    int slot_tmp = thread->cur_slot;
    thread->cur_slot = slot;
#endif
	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }
#ifdef VERIFICATION
    thread->cur_slot = slot_tmp;
#endif
}



void hmx_wgt_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int weight_count, int output_ch_scale)
{
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
	memset(hmx_maptr, 0, sizeof(hmx_mem_access_info_t));

	paddr_t align_128_mask = 0x7F;
	int length = (int)range & (~align_128_mask);
	int start_aligned = (int)start & (~align_128_mask);
	mem_init_access_unaligned(thread, slot, start, start, length, access_type_hmx_load_wei, TYPE_LOAD);

	if (EXCEPTION_DETECTED) return;

	paddr = maptr->paddr;
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
	CHECK_ACCESS_RANGE(mxmem_exception, paddr, length);
	mxmem_exception |= check_mxmem_page_cross(thread, start_aligned , length, thread->mem_access[slot].xlate_info.size);
	if (mxmem_exception) {
       	register_coproc_ldst_exception(thread,slot,start);
	}

	if (EXCEPTION_DETECTED) return;

	maptr->width = length;
	maptr->vaddr = start;

	uint32_t vtcm_addr_mask = thread->processor_ptr->arch_proc_options->vtcm_original_mem_entries * thread->processor_ptr->arch_proc_options->QDSP6_VX_VEC_SZ/8 - 1;

	maptr->range = range;

	paddr_t last_wgt_mask = (thread->processor_ptr->arch_proc_options->hmx_output_depth * 4) - 1;
	paddr_t wgt_align_mask = ~last_wgt_mask;

	paddr = paddr & wgt_align_mask;
	maptr->paddr = paddr;
	THREAD2HMXSTRUCT->wgt_addr = maptr->paddr;
	paddr_t wgt_pa_at_mac_limit = paddr;
	THREAD2HMXSTRUCT->max_weight_pa = (paddr + ( range & vtcm_addr_mask )) | last_wgt_mask;

	weight_count = (output_ch_scale==2) ? (weight_count-1) :  weight_count;	// Move Elsewhere?

	paddr_t limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	limit = limit * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	limit = limit * 9 / 8; //for density=127 weight decompression case, we need to increase the limit to 576 vectors

	int wgtc_mode = (start & 0x10)>>4;
	if(wgtc_mode==0)
		limit >>= weight_count;

	if ( ((range & vtcm_addr_mask) | last_wgt_mask) >= limit) {
		fMX_DEBUG_LOG(0, "Weight size limit exceeded: %llx limit: %llx ", (range | last_wgt_mask), limit);
		THREAD2HMXSTRUCT->max_weight_pa = paddr + limit - 1 ;
	}

	int32_t fxp_max_wgt = 4 * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	int32_t flt_max_wgt = 2 * thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_RATE * thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_COLS;
	wgt_pa_at_mac_limit += (THREAD2HMXSTRUCT->mac_cycle_limit * ((THREAD2HMXSTRUCT->is_flt) ? flt_max_wgt  : fxp_max_wgt )) >>  weight_count;
	wgt_pa_at_mac_limit -= 1;

	if ((wgt_pa_at_mac_limit < THREAD2HMXSTRUCT->max_weight_pa) && (wgtc_mode != 1)) {
		THREAD2HMXSTRUCT->max_weight_pa = wgt_pa_at_mac_limit ;
		fMX_DEBUG_LOG(0, "Weight size limit exceeded: %llx last_wgt_mask: %llx max_weight_pa %llx limit %llx", wgt_pa_at_mac_limit, last_wgt_mask, THREAD2HMXSTRUCT->max_weight_pa, limit );
	}

	THREAD2HMXSTRUCT->operand_ready |= ((int)range>=0)<<slot;

	if ((((start & wgt_align_mask & vtcm_addr_mask) + (range & wgt_align_mask & vtcm_addr_mask)) & vtcm_addr_mask) < (start & wgt_align_mask & vtcm_addr_mask)) {
		THREAD2HMXSTRUCT->operand_ready = 0;
		warn("HMX MAC WGT Start + Range wraps around so packet dropped");
		fMX_DEBUG_LOG(0, "HMX MAC WGT Start + Range wraps around so packet dropped");
	}

#ifdef VERIFICATION
    int slot_tmp = thread->cur_slot;
    thread->cur_slot = slot;
#endif
	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }
#ifdef VERIFICATION
    thread->cur_slot = slot_tmp;
#endif

}

#define ESR_FPCOPROC USR_FPCOPROC
uint32_t hmx_get_usr_reg_coproc_field(thread_t* thread) {
	return GET_FIELD(ESR_FPCOPROC, thread->Regs[REG_USR]);
}


void hmx_mxmem_wr_xlate(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int access_type)
{
    paddr_t paddr;
    paddr_t tile_mask = (1 << (thread->processor_ptr->arch_proc_options->hmx_spatial_size + fMX_GETCHANNELSIZE(thread->processor_ptr)))-1;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	vaddr_t dY =  range & ~tile_mask;

	// Exception Checking
	MX_EXCEPTION_CHECK(start, dY, dY, access_type, TYPE_STORE);

	paddr = paddr & ~tile_mask;

	maptr->paddr = paddr;
	maptr->width = 2048;

	fVDOCHKPAGECROSS(start, start + dY);
#ifdef VERIFICATION
    int slot_tmp = thread->cur_slot;
    thread->cur_slot = slot;
#endif
	if(!thread->bq_on) { memwrap_memtrace_st(thread, thread->Regs[REG_PC], start, paddr, 1); }
#ifdef VERIFICATION
    thread->cur_slot = slot_tmp;
#endif
}

void hmx_debug_file_log(thread_t* thread, int lvl, char * buf) {
	if (thread->processor_ptr->arch_proc_options->hmxdebugfile && (thread->processor_ptr->arch_proc_options->hmxdebuglvl >= lvl) && !thread->bq_on) {
		fprintf(thread->processor_ptr->arch_proc_options->hmxdebugfile, "%s\n", buf);
	}
}



void hmx_debug_print_acc(thread_t* thread, int hex, int flt ) {

	fprintf(stdout, "Archsim accumulator debug print\n");
	for(int32_t acc_idx = 0; acc_idx < 2; acc_idx++)
	{
		fprintf(stdout, "Accumulator[%d]:\n", acc_idx);
		const int32_t inc = (flt) ? 2 : 1;
		for (int32_t i=0; i<MAX_ACCUMULATORS_SPATIAL; i+=inc)
		{
			const int32_t L = (flt || (thread->processor_ptr->arch_proc_options->QDSP6_MX_SUB_COLS>1)) ? 16 : 8;
			const int32_t K = MAX_ACCUMULATORS_DEPTH/L;
			for(int32_t l = 0; l < L/2; l++)
			{
				fprintf(stdout, "Channel:     ");
				for (int32_t j=K*l; j < K*l+K; j++) {
					if (flt) {
						if (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP) {
							fprintf(stdout, "%39d", j);
						} else {
							fprintf(stdout, "%33d ", j);
						}
					} else {
						if (thread->processor_ptr->arch_proc_options->QDSP6_MX_SUB_COLS>1) {
							fprintf(stdout, "%25d ", j);
						} else {
							fprintf(stdout, "%8d ", j);
						}
					}
				}
				fprintf(stdout,"\n");
				fprintf(stdout, "Spatial: %02d: ", i);
				for (int32_t j=K*l; j < K*l+K; j++)
				{
					if (flt) {
						uint16_t exp = THREAD2HMXSTRUCT->accum_flt[i][j].xfp[acc_idx].exp;
						long long int sig = THREAD2HMXSTRUCT->accum_flt[i][j].xfp[acc_idx].sig;
						uint16_t inf = THREAD2HMXSTRUCT->accum_flt[i][j].xfp[acc_idx].status.inf;
						uint16_t z = THREAD2HMXSTRUCT->accum_flt[i][j].xfp[acc_idx].status.zero;
						uint16_t n = THREAD2HMXSTRUCT->accum_flt[i][j].xfp[acc_idx].status.negative;
						fprintf(stdout, "exp:%04x sig:%08x inf:%1x zero:%1x neg:%1x ", exp, (int32_t)sig, inf, z, n );
					} else {
						int32_t hi = (int32_t)THREAD2HMXSTRUCT->accum_fxp[i][j].w[acc_idx+0];
						int32_t lo = (int32_t)THREAD2HMXSTRUCT->accum_fxp[i][j].w[acc_idx+2];
						if (thread->processor_ptr->arch_proc_options->QDSP6_MX_SUB_COLS>1) {
							fprintf(stdout, ".hi=%08x .lo=%08x ", hi, lo);
						} else {
							fprintf(stdout, "%08x ", hi);
						}
					}
				}
				fprintf(stdout,"\n");
			}
			fprintf(stdout,"\n");
		}
		fprintf(stdout,"\n");
	}

	fflush(stdout);
}
