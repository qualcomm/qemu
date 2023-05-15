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
#include "hmx/hmx_utils.h"
#include "hmx/hmx_hex_arch_types.h"
#include "hmx/hmx_coproc.h"
#include "hmx/hmx_system.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx.h"
#include "hmx/macros_auto.h"

#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)
#define INC_TSTAT(...)
#define INC_TSTATN(...)
#define INC_TSTATNPC(...)
#define INC_PSTATNPC(...)

#ifdef CONFIG_USER_ONLY
#define CPU_MMU_INDEX(ENV) MMU_USER_IDX
#else
#define CPU_MMU_INDEX(ENV) cpu_mmu_index((ENV), false)
#endif

#ifdef HEX_CONFIG_INT128
static inline size4u_t int128_getword(size16s_t data, int word_select)

{
    size4u_t result;

    switch (word_select) {
        case 0:
            result = int128_getlo(data);
            break;
        case 1:
            result = int128_getlo(data) >> 32;
            break;
        case 2:
            result = int128_gethi(data);
            break;
        case 3:
            result = int128_gethi(data) >> 32;
        break;
        default:
            g_assert_not_reached();
    }

    return result;
}
#endif

static void clear_xfp_accumulators(hmx_state_t * state, int idx) {
	hmx_xfp_t xfp_zero = hmx_xfp_zero(state);
	for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
		for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
				state->accum_flt[y][x].xfp[idx] = xfp_zero;
				state->accum_flt[y][x].ovf[idx] = 0;
		}
	}
}
static void clear_fxp_accumulators(hmx_state_t *hmx_state, int acc_idx) {
	for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
		for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
			hmx_state->accum_fxp[y][x].w[acc_idx+0] = hmx_state->internal_bias_value;	// Lo
			hmx_state->accum_fxp[y][x].w[acc_idx+2] = hmx_state->internal_bias_value;	// Hi
			hmx_state->accum_fxp[y][x].bias_state |= (1<<(acc_idx+0));
			hmx_state->accum_fxp[y][x].bias_state |= (1<<(acc_idx+2));
		}
	}
}

void hmx_ext_alloc(processor_t *proc, int slots) { }
void *hmx_ext_talloc(processor_t *proc, int slots) { return NULL; }
void hmx_ext_init(processor_t *proc, int extno) {
	thread_t *thread = proc->thread[0];
	hmx_state_t * state = THREAD2HMXSTRUCT;
	hmx_tmp_set_state(thread, state); // Configure internal parameters
	state->internal_bias_value = (state->QDSP6_MX_SUB_COLS>1) ? -(1 << 16) : -(1 << 20); // TODO: Make me a parameter
	clear_xfp_accumulators(state,0);
	clear_xfp_accumulators(state,1);
}


int hmx_ext_decode_checks(thread_t *thread, Packet *pkt,
                          hex_exception_info *einfo)
{
    return 0;
}
const char * hmx_ext_decode_find_iclass_slots(int opcode) { return ""; }

void hmx_ext_print_reg(thread_t *thread, FILE *fp, int rnum, int extno)
{
	hmx_state_t * state = THREAD2HMXSTRUCT;
	int print_col = 4;

	if(rnum >= state->QDSP6_MX_ROWS) {
		fprintf(fp, "HMX row index is out of bounds, max is %d", state->QDSP6_MX_ROWS-1);
		return;
	}
	fprintf(fp, "\tSet 0\t\t\tSet 1\n");
	for (int j=0;j<state->QDSP6_MX_COLS; j+=print_col) {
		fprintf(fp,"A%02d.%02d: ", rnum, j);
		for(int k=0; k < print_col && (j + k) < state->QDSP6_MX_COLS; k++) {
			fprintf(fp, "%04x ", (int32_t)state->accum_fxp[rnum][j+k].w[0]);

		}
		fprintf(fp, "\t");
		for(int k=0; k < print_col && (j + k) < state->QDSP6_MX_COLS; k++) {
			fprintf(fp, "%04x ", (int32_t)state->accum_fxp[rnum][j+k].w[1]);
		}
		fprintf(fp, "\n");
	}

}
void hmx_ext_print_regs(thread_t *thread, FILE *fp, int extno) {
	hmx_state_t * state = THREAD2HMXSTRUCT;
	int print_col = 4;
	fprintf(fp, "\nAccumulators size configured as %d <spatial size> by %d <channel depth>\n", state->QDSP6_MX_ROWS, state->QDSP6_MX_COLS);
	fprintf(fp, "\tSet 0\t\t\tSet 1\n");
	for (int i=0;i<state->QDSP6_MX_ROWS;i++) {
		for (int j=0;j<state->QDSP6_MX_COLS; j+=print_col) {
			fprintf(fp,"A%02d.%02d: ", i, j);
			for(int k=0; k < print_col && (j + k) < state->QDSP6_MX_COLS; k++) {;
				fprintf(fp, "%04x ", (int32_t)state->accum_fxp[i][j+k].w[0]);
			}

			fprintf(fp, "\t");
			for(int k=0; k < print_col && (j + k) < state->QDSP6_MX_COLS; k++) {
				fprintf(fp, "%04x ", (int32_t)state->accum_fxp[i][j+k].w[1]);
			}
			fprintf(fp, "\n");
		}
	}
	fprintf(fp,"\n");
}

int hmx_ext_get_ovf(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t acc_select, size4u_t *result)
{
	*result = 0x0;
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t * state = THREAD2HMXSTRUCT;
		spatial_index <<= 1;
		*result = state->accum_flt[spatial_index][channel_index].ovf[acc_select];
		fMX_DEBUG_LOG(2, "TB HMX READ: FLT OVERFLOW BITS ACC[%02d][%02d][%02d]=%08x", spatial_index/2, channel_index, acc_select, *result);
		return 0;
	}

	return -1;
}

int hmx_ext_set_ovf(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t acc_select, size4u_t val)
{
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t * state = THREAD2HMXSTRUCT;
		spatial_index <<= 1;
		state->accum_flt[spatial_index][channel_index].ovf[acc_select] = val;
		fMX_DEBUG_LOG(2, "TB HMX WRITE: FLT OVERFLOW BITS ACC[%02d][%02d][%02d]=%08x", spatial_index/2, channel_index, acc_select, val);
		return 0;
	}
	return -1;
}

void *hmx_ext_palloc(processor_t *proc, int slots)
{
	hmx_state_t *hmx_state;
	if ((hmx_state = (hmx_state_t *) calloc(1,sizeof(hmx_state_t))) == NULL) {
		assert(-1);
	}

	return hmx_state;
}

void hmx_ext_pfree(processor_t *proc, int xa, int slots)
{
	free(proc->shared_extptr);
	proc->shared_extptr = NULL;
}
void hmx_ext_tfree(processor_t *proc, int xa, int slots)
{
}
// TODO: Take state as input not proc
void hmx_acc_ptr_reset(processor_t *proc) {
	thread_t * thread = proc->thread[0];
	hmx_state_t * state = THREAD2HMXSTRUCT;
	state->current_acc_flt = 0;
	state->current_acc_fxp = 0;
}

void hmx_reset(processor_t *proc, thread_t *thread){
	hmx_acc_t reset_acc;
	hmx_acc_t reset_acc_zero;
	hmx_acc_t reset_acc_flt;
	hmx_bias_t reset_bias;
	hmx_state_t * state = THREAD2HMXSTRUCT;


	reset_bias.val[0] = 0;
	reset_bias.val[1] = 0;
	reset_acc.ovf[0] = 0;
	reset_acc.ovf[1] = 0;
	reset_acc_zero.ovf[0] = 0;
	reset_acc_zero.ovf[1] = 0;
	reset_acc_flt.ovf[0] = 0;
	reset_acc_flt.ovf[1] = 0;
	for(int i = 0; i < MAX_HMX_ACC_BYTES/4; i++){
		reset_acc.uw[i] = state->internal_bias_value;
		reset_acc_zero.uw[i] = 0;
		reset_acc_flt.uw[i] = 0;
	}

	for(int acc_idx = 0; acc_idx < 2; acc_idx++) {
		reset_acc_flt.xfp[acc_idx].exp = (-1<<(state->QDSP6_MX_FP_ACC_EXP-1));
		reset_acc_flt.xfp[acc_idx].sig = 0;
		reset_acc_flt.xfp[acc_idx].status.inf = 0;
		reset_acc_flt.xfp[acc_idx].status.negative = 0;
		reset_acc_flt.xfp[acc_idx].status.zero = 1;
		reset_acc_flt.xfp[acc_idx].EXP = state->QDSP6_MX_FP_ACC_EXP;
		reset_acc_flt.xfp[acc_idx].INT = state->QDSP6_MX_FP_ACC_INT;
		reset_acc_flt.xfp[acc_idx].FRAC = state->QDSP6_MX_FP_ACC_FRAC;
	}

	for(int depth = 0; depth < MAX_ACCUMULATORS_DEPTH; depth++){
		for(int spatial = 0; spatial < MAX_ACCUMULATORS_SPATIAL; spatial++){
			state->accum_fxp[spatial][depth] = reset_acc;
			state->accum_flt[spatial][depth] = reset_acc_flt;
			state->cvt_future_accum_fxp[spatial][depth] = reset_acc_zero;
			state->cvt_fallback_future_accum_fxp[spatial][depth] = reset_acc_zero;
			state->cvt_future_accum_flt[spatial][depth] = reset_acc_zero;
			state->cvt_fallback_future_accum_flt[spatial][depth] = reset_acc_zero;
			state->future_accum_fxp[spatial][depth] = reset_acc_zero;
			state->future_accum_flt[spatial][depth] = reset_acc_flt;
			state->accum_fxp[spatial][depth].bias_state = 0xF;
			for(int cvt_state = 0; cvt_state < MAX_CONVERT_STATES; cvt_state++){
				state->cvt_accum[cvt_state][spatial][depth] = reset_acc_zero;

			}
		}
	}

	for(int depth = 0; depth < MAX_ACCUMULATORS_DEPTH/2; depth++){
		for(int bias_num = 0; bias_num < MAX_BIAS_STATES; bias_num++){
			state->bias[bias_num][depth] = reset_bias;
			state->future_bias[bias_num][depth] = reset_bias;
		}
    }

	state->current_acc_flt = 0;
	state->current_acc_fxp = 0;
}

/* Commit registers */
void hmx_ext_commit_regs(thread_t *thread, int extno)
{
	hmx_state_t * state = THREAD2HMXSTRUCT;
	state->operand_ready = 0;
	// Fixed Point Array
	if(state->fxp_commit_state.cvt_update) {
		if(!state->fxp_commit_state.cvt_advance){
			hmx_age_cvt_state(thread);
		}
		memcpy(state->cvt_accum[state->cvt_accum_current_index], state->cvt_future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memcpy(state->cvt_fallback_future_accum_fxp, state->cvt_future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);

	}
	if(state->fxp_commit_state.acc_update) {
		memcpy(state->accum_fxp, state->future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);

	}
	if(state->fxp_commit_state.bias_update) {
		memcpy(state->bias,  state->future_bias,  MAX_BIAS_STATES*sizeof(hmx_bias_t)*MAX_ACCUMULATORS_DEPTH/2);
	}

	state->fxp_commit_state.cvt_update = 0;
	state->fxp_commit_state.cvt_advance = 0;
	state->fxp_commit_state.acc_update = 0;
	state->fxp_commit_state.bias_update = 0;


	// Floating Point Array
	if(state->flt_commit_state.cvt_update) {
		if(!state->flt_commit_state.cvt_advance){
			hmx_age_cvt_state(thread);
		}
		memcpy(state->cvt_accum[state->cvt_accum_current_index], state->cvt_future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memcpy(state->cvt_fallback_future_accum_flt, state->cvt_future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}
	if(state->flt_commit_state.acc_update) {
		if(state->support_fp16 == 0){
				clear_xfp_accumulators(state,0);
		}
		else{
			memcpy(state->accum_flt, state->future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		}
	}


	state->flt_commit_state.cvt_update = 0;
	state->flt_commit_state.cvt_advance = 0;
	state->flt_commit_state.acc_update = 0;

	if(state->fxp_commit_state.acc_clear_both) {
		state->fxp_commit_state.acc_clear_both = 0;
		clear_fxp_accumulators(state, 0);
		clear_fxp_accumulators(state, 1);
		memset(state->accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memset(state->future_accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}
	if(state->flt_commit_state.acc_clear_both) {
		state->flt_commit_state.acc_clear_both = 0;
		clear_xfp_accumulators(state,0);
		clear_xfp_accumulators(state,1);
		memset(state->future_accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}

	if(state->fxp_commit_state.swap_acc) {
        state->current_acc_fxp ^= 1;
		state->fxp_commit_state.swap_acc = 0;
	}
	if(state->flt_commit_state.swap_acc) {
		state->current_acc_flt ^= 1;
		state->flt_commit_state.swap_acc = 0;
	}
}


/* Cancel packet in progress */
void hmx_ext_rewind(thread_t *thread, int extno)
{
	hmx_state_t *state = THREAD2HMXSTRUCT;
	state->operand_ready = 0;
	if(state->fxp_commit_state.cvt_update)
		memcpy(state->cvt_future_accum_fxp, state->cvt_fallback_future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	if(state->flt_commit_state.cvt_update)
		memcpy(state->cvt_future_accum_flt, state->cvt_fallback_future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);

	if(state->fxp_commit_state.bias_update) {
		memcpy(state->future_bias,  state->bias,  MAX_BIAS_STATES*sizeof(hmx_bias_t)*MAX_ACCUMULATORS_DEPTH/2);
	}
	state->fxp_commit_state.val = 0;
	state->flt_commit_state.val = 0;
};


void hmx_ext_commit_rewind(thread_t *thread)
{
	hmx_state_t *state = THREAD2HMXSTRUCT;
	state->operand_ready = 0;
	state->fxp_commit_state.val = 0;
	state->flt_commit_state.val = 0;
}

static void write_converted_peg(thread_t *thread, int x_idx, int y_idx, int x_acc_idx, int y_acc_idx, int old_state) {
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	int acc_idx = (x_acc_idx|y_acc_idx);

	int format =  hmx_state->format;
	int flt = hmx_state->is_flt;
	int enable16x16 = hmx_state->enable16x16;
	int outputselect16x16 = hmx_state->outputselect16x16;
	paddr_t paddr_base = thread->mem_access[0].paddr;

	fMX_GET_ACC_INDEX(acc_idx, format);

	int beginning = 0;
	int end = hmx_state->QDSP6_MX_COLS;
	int offset_16x16 = 0;
	int scale_16x16 = 1;
	int adjust_16x16 = 0;
	//modifying to only print 16 outputs
	if(enable16x16){
		adjust_16x16 = 1;
		scale_16x16 = 2;
		if(outputselect16x16){
			beginning = hmx_state->QDSP6_MX_COLS / 2;
			offset_16x16 = 16;
		}
		else{
			end = hmx_state->QDSP6_MX_COLS / 2;
		}
	}

	int cvt_accum_index = hmx_state->cvt_accum_current_index;

	if (old_state)
	{
		cvt_accum_index += MAX_CONVERT_STATES; //Force backwards iteration to be positive
		cvt_accum_index = (cvt_accum_index - 1) % MAX_CONVERT_STATES;
	}

	int convert_width = hmx_state->QDSP6_MX_CVT_WIDTH;
	if((convert_width < 8) || (convert_width > 16)){
		fatal("convert state width does not fall in acceptable range");
	}

	for(int z_idx = beginning; z_idx < end; z_idx++) {

		paddr_t pa = paddr_base + (x_idx|y_idx) + (z_idx << format);
		//fMX_DEBUG_LOG(2,        "HMX_CVT_FLT: pa=%llx x=%x y=%x z=%x" , pa, x_idx, y_idx, (z_idx << format));
		size1u_t byte = 0;
		size2u_t hf = 0;
		int z_idx_adjusted = ((z_idx - offset_16x16) * scale_16x16) + adjust_16x16;
		if(flt) {
			size1u_t byte_zero = (hmx_state->cvt_accum[cvt_accum_index][acc_idx][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
			size1u_t byte_one  = (hmx_state->cvt_accum[cvt_accum_index][acc_idx + 1][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
			hf = byte_zero + (((size2u_t)byte_one) << 8);
			if (thread->processor_ptr->options->hmx_cvt_state_write_callback) {
				thread->processor_ptr->options->hmx_cvt_state_write_callback(thread->system_ptr, thread->processor_ptr, thread->pktid, old_state, acc_idx, z_idx, acc_idx, byte_zero);
				thread->processor_ptr->options->hmx_cvt_state_write_callback(thread->system_ptr, thread->processor_ptr, thread->pktid, old_state, acc_idx+1, z_idx, acc_idx, byte_one);
			}
			sim_mem_write2(thread->system_ptr,thread->threadId, pa, hf);
			fMX_DEBUG_LOG(2,        "HMX_CVT_FLT: WRITE CVT_STATE[%02d][%02d] PA=%08llx Half word: %04x pktid:%08x" , acc_idx+0, z_idx_adjusted, pa+0, byte_zero, thread->pktid);
			fMX_DEBUG_LOG(2,        "HMX_CVT_FLT: WRITE CVT_STATE[%02d][%02d] PA=%08llx Half word: %04x pktid:%08x" , acc_idx+1, z_idx_adjusted, pa+1, byte_one, thread->pktid);
		} else {
			byte = (hmx_state->cvt_accum[cvt_accum_index][acc_idx][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
			if (thread->processor_ptr->options->hmx_cvt_state_write_callback) {
				thread->processor_ptr->options->hmx_cvt_state_write_callback(thread->system_ptr, thread->processor_ptr, thread->pktid, old_state, acc_idx, z_idx, acc_idx, byte);
			}
			sim_mem_write1(thread->system_ptr,thread->threadId, pa, byte);
			fMX_DEBUG_LOG(2,         "HMX_CVT_FXP: WRITE CVT_STATE[%02d][%02d] PA=%08llx Byte: %02x pktid:%08x" , acc_idx, z_idx_adjusted, pa, byte, thread->pktid);
		}

#ifdef VERIFICATION
		int slot_tmp = thread->cur_slot;
		thread->cur_slot = 0;
		int width = (flt) ? 2 : 1;
		int val = (flt) ? hf : byte;
		if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
			thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, width, DWRITE, val);
		}
		thread->cur_slot = 0;
		thread->cur_slot = slot_tmp;
#endif
	}
}


static void write_x_row(thread_t *thread, int y_idx, int y_acc_idx) {
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	int x_idx  = 0;
	int x_acc_idx = hmx_state->x_acc_offset;
	int x_inc = hmx_state->tile_x_inc;
	int x_mask = hmx_state->tile_x_mask | (1<<31);
	int x_offset  = hmx_state->x_offset;	// Start at offset

	//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: starting x=%x acc=%x offset = %x y=%x offset = %x pktid:%08x" , x_idx, x_acc_idx, x_offset, y_idx, y_acc_idx, thread->pktid);
	for (; x_idx < x_offset; ) {
		if ((hmx_state->cvt_type == HMX_CVT_BEFORE) || (hmx_state->cvt_type == HMX_CVT_BOTH)) {
			//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: before: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx, hmx_state->cvt_type == HMX_CVT_BOTH);
			x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, x_inc, x_mask);
		}
		x_idx = hmx_inc_with_spatial_mask(x_idx, x_inc, x_mask);
		if (x_inc == 0) break;
	}
	//printf("x_idx=%x x_acc_idx=%x\n", x_idx, x_acc_idx);
	x_acc_idx = 0;

	while(x_idx>=0) {
		if ((hmx_state->cvt_type == HMX_CVT_AFTER) || (hmx_state->cvt_type == HMX_CVT_BOTH)) {
			//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: after: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx, 0);
			x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, x_inc, x_mask);
		}
		x_idx = hmx_inc_with_spatial_mask(x_idx, x_inc, x_mask);
		if (x_inc == 0) break;
	}
}

static void write_out_convert(thread_t *thread) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		int y_mask = hmx_state->tile_y_mask | (1<<31);
		int y_inc  = hmx_state->tile_y_inc;
		int y_idx  = hmx_state->y_offset;
		int dY = hmx_state->dY;
		int y_offset = y_idx;
		int y_acc_idx  = 0;

		while(y_idx>=0) {
			// Write out Horizontal Strip
			//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: writing row offset y=%x pktid:%08x" , y_idx, thread->pktid);
			write_x_row(thread, y_idx, y_acc_idx);
			y_idx =  hmx_inc_with_spatial_mask(y_idx, y_inc, y_mask);
			y_acc_idx =  hmx_inc_with_spatial_mask(y_acc_idx, y_inc, y_mask);
			if (y_inc == 0) break;
		}
		if (dY != 0) {
			// Address to next tile
			thread->mem_access[0].paddr += dY;
			//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: second block writing row y=%x pktid:%08x" , y_idx, thread->pktid);
			for (y_idx  = 0; y_idx < y_offset; )  {
				write_x_row(thread, y_idx, y_acc_idx);
				y_idx = hmx_inc_with_spatial_mask(y_idx, y_inc, y_mask);
				y_acc_idx = hmx_inc_with_spatial_mask(y_acc_idx, y_inc, y_mask);
				if (y_inc == 0) break;
			}
		} else {
			for (y_idx  = 0; y_idx < y_offset; )  {
				//fMX_DEBUG_LOG(1,  "HMX_CVT_FLT: writing row y=%x pktid:%08x" , y_idx, thread->pktid);
				write_x_row(thread, y_idx, y_acc_idx );
				y_idx = hmx_inc_with_spatial_mask(y_idx, y_inc, y_mask);
				y_acc_idx =  hmx_inc_with_spatial_mask(y_acc_idx, y_inc, y_mask);
				if (y_inc == 0) break;
			}
		}
}


void hmx_ext_commit_mem(thread_t *thread, int slot, int extno)
{
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	// NPU: Matrix Multiply Accumulators

	if(hmx_state->fxp_commit_state.cvt_write) {
		write_out_convert(thread);
		hmx_state->fxp_commit_state.cvt_write = 0;
		memset(hmx_state->future_accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}

	if (hmx_state->fxp_commit_state.acc_clear) {
		int current_acc = hmx_state->acc_select;
		clear_fxp_accumulators(hmx_state, current_acc);
		hmx_state->current_acc_fxp ^= 0x1 ; // Flip Accumulator
		hmx_state->fxp_commit_state.acc_clear = 0;
#ifdef VERIFICATION
		fMX_DEBUG_LOG(1, "\t HMX CVT Accumulator swapped to %x", hmx_state->fxp_commit_state.acc_clear);
#endif
	}

	// Float Array
	if(hmx_state->flt_commit_state.cvt_write) {
		write_out_convert(thread);
		hmx_state->flt_commit_state.cvt_write = 0;
		memset(hmx_state->future_accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}

	if (hmx_state->flt_commit_state.acc_clear) {
		clear_xfp_accumulators(hmx_state, hmx_state->acc_select);
		hmx_state->flt_commit_state.acc_clear = 0;
		hmx_state->current_acc_flt ^= 0x1 ; // Flip Accumulator
#ifdef VERIFICATION
		fMX_DEBUG_LOG(1, "\t HMX CVT Accumulator swapped to %x", hmx_state->fxp_commit_state.acc_clear);
#endif
	}

	// This will get cleaned up when old bias instructions are replicated
	if(hmx_state->fxp_commit_state.bias_write) {
		for(int output_ch_idx = 0; output_ch_idx < hmx_state->QDSP6_MX_COLS; output_ch_idx++) {
			size4u_t temp = hmx_state->bias[hmx_state->fxp_commit_state.bias_sel][output_ch_idx].val[0];
			paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx;
			sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
#ifdef VERIFICATION
			int slot_tmp = thread->cur_slot;
			thread->cur_slot = 0;
			if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
				thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DWRITE, temp);
			}
			thread->cur_slot = slot_tmp;
#endif
		}
		if(hmx_state->fxp_commit_state.bias_write == 2) {
			for(int output_ch_idx = 0; output_ch_idx < hmx_state->QDSP6_MX_COLS; output_ch_idx++) {
				size4u_t temp = hmx_state->bias[hmx_state->fxp_commit_state.bias_sel][output_ch_idx].val[1];
				paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx + 128;
				sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
#ifdef VERIFICATION
				int slot_tmp = thread->cur_slot;
				thread->cur_slot = 0;
				if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
					thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DWRITE, temp);
				}
				thread->cur_slot = slot_tmp;
#endif
			}
		}
		hmx_state->fxp_commit_state.bias_write = 0;
		hmx_state->fxp_commit_state.bias_sel = 0;
	}


}

void hmx_ext_print_acc(thread_t *thread, FILE *fp);
void hmx_ext_print_acc(thread_t *thread, FILE *fp) {

}

int	hmx_ext_set_fp_acc(thread_t *thread,  int spatial_idx, int channel_idx, int acc_idx, size4s_t exponent, size8s_t significand_hi, size8u_t significand_lo, size4u_t ovf) {

	if (THREAD2HMXSTRUCT != NULL)
	{
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		spatial_idx <<= 1;

		hmx_xfp_t acc;
		int32_t shift = hmx_state->QDSP6_MX_FP_ACC_FRAC+hmx_state->QDSP6_MX_FP_ACC_INT;
		acc.EXP = hmx_state->QDSP6_MX_FP_ACC_EXP;

		// mask
		acc.exp = exponent & ((1<<hmx_state->QDSP6_MX_FP_ACC_EXP)-1);

		// sign extend
		acc.exp <<= (32 - hmx_state->QDSP6_MX_FP_ACC_EXP);
		acc.exp >>= (32 - hmx_state->QDSP6_MX_FP_ACC_EXP);

		acc.FRAC = hmx_state->QDSP6_MX_FP_ACC_FRAC;
		acc.INT = hmx_state->QDSP6_MX_FP_ACC_INT;
		acc.sig = significand_hi >> (64 - shift );
		acc.status.val = 0;
		acc.status.inf = ovf ;
		acc.lza = 0;
		if (acc.status.inf==0) {
			acc.status.negative = (significand_hi < 0);
			int32_t min_exp = -(1 << (acc.EXP-1));
			acc.status.zero = (significand_hi==0) && (acc.exp == min_exp);
		} else {
			acc.status.negative = ((acc.status.inf & 0x2) == 2);
		}
		hmx_state->accum_flt[spatial_idx][channel_idx].ovf[acc_idx] = ovf;
		hmx_state->accum_flt[spatial_idx][channel_idx].xfp[acc_idx] = acc;

		fMX_DEBUG_LOG(2,  "TB HMX WRITE: XFP ACC[%02d][%02d][%02d] exp: %04x raw sig: %016llx q=%02x.%06x Q%d.%de%d status inf: %d neg: %d zero: %d -> legacy: in ovf: %d exp: 0x%04x sig=%016llx.%016llx",
		spatial_idx/2, channel_idx, acc_idx, (uint16_t) acc.exp, (long long int)acc.sig, (uint8_t)((acc.sig >> acc.FRAC) & 0xFF), (uint32_t)(((acc.sig) & ((1<<acc.FRAC)-1)) << (24 - acc.FRAC)), acc.INT, acc.FRAC, acc.EXP,  acc.status.inf, acc.status.negative, acc.status.zero, ovf,  exponent, significand_hi, significand_lo );

		return 0;
	}
	return -1;

}

int hmx_ext_get_fp_acc(thread_t *thread,  int spatial_idx, int channel_idx, int acc_idx, size4s_t * exponent, size8s_t * significand_hi, size8u_t * significand_lo, size4u_t * ovf) {
	*significand_hi = 0xDEADBEEF;
	*significand_lo = 0xDEADBEEF;
	*ovf = 0xDEADBEEF;
	if (THREAD2HMXSTRUCT != NULL)
	{
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		spatial_idx <<= 1;
		*exponent = 0;
		hmx_xfp_t acc = hmx_state->accum_flt[spatial_idx][channel_idx].xfp[acc_idx];
		size16s_t acc_128 = hmx_xfp_to_tb_callback(hmx_state, exponent, ovf, acc);
#ifdef HEX_CONFIG_INT128
        *significand_hi = int128_gethi(acc_128);
        *significand_lo = int128_getlo(acc_128);
#else
 		*significand_hi = acc_128.hi;
 		*significand_lo = acc_128.lo;
#endif
		fMX_DEBUG_LOG(2,  "TB HMX READ: XFP ACC[%02d][%02d][%02d] exp: %08x sig: %016llx status inf: %d neg: %d true zero: %d ovf: %d exponent: %08x significand=%016llx%016llx ",
			spatial_idx/2, channel_idx, acc_idx, acc.exp,  (long long int)acc.sig, acc.status.inf, acc.status.negative,  acc.status.zero, *ovf, *exponent, *significand_hi, *significand_lo);


		return 0;
	}
	return -1;

}

int hmx_ext_get_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_index, size8s_t * integer, size8u_t * fractional, size4u_t * ovf) {
	*integer = 0xDEADBEEF;
	*fractional = 0xDEADBEEF;
	*ovf = 0xDEADBEEF;
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		spatial_idx <<= 1;
#ifdef HEX_CONFIG_INT128
        *integer = int128_gethi(hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index]);
        *fractional = int128_getlo(hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index]);
#else
 		*integer = hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].hi;
 		*fractional = hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].lo;
#endif
		*ovf = hmx_state->accum_flt[spatial_idx][channel_idx].ovf[acc_index];
		return 0;
	}
	return -1;
}
int hmx_ext_set_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_index, size8s_t integer, size8u_t fractional, size4u_t ovf)
{

	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		spatial_idx <<= 1;
#ifdef HEX_CONFIG_INT128
		hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index] =
            int128_make128(fractional, integer);
#else
 		hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].hi = integer;
 		hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].lo = fractional;
#endif
		hmx_state->accum_flt[spatial_idx][channel_idx].ovf[acc_index] = ovf;
		fMX_DEBUG_LOG(2,  "TB HMX WRITE: FLT ACC[%02d][%02d][%02d] = %016llx.%016llx ovf=%x",
			spatial_idx/2, channel_idx, acc_index,
			hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].hi,
			hmx_state->accum_flt[spatial_idx][channel_idx].val[acc_index].lo,
			hmx_state->accum_flt[spatial_idx][channel_idx].ovf[acc_index] );
		return 0;
	}
	return -1;
}

int hmx_ext_get_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		int acc_select = (wordno > 3);
		int word_select = wordno & 0x3;
		int acc_shift = 16+66 - (hmx_state->QDSP6_MX_FP_ACC_FRAC+hmx_state->QDSP6_MX_FP_ACC_INT);
		spatial_index <<= 1;
		size16s_t acc =  hmx_state->accum_flt[spatial_index][channel_index].val[acc_select];

		acc = shiftr128(acc, acc_shift);
#ifdef HEX_CONFIG_INT128
        *result = int128_getword(acc, word_select);
#else
 		*result = acc.w[word_select];
#endif
		fMX_DEBUG_LOG(2, "TB HMX READ: FLT ACC[%02d][%02d][%02d].w[%d]=%08x pktid:%08x", spatial_index/2, channel_index, acc_select, wordno, *result, thread->pktid);


		return 0;
	}
	return -1;
}
int hmx_ext_set_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val) {

	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		int acc_select = (wordno > 3);
		int word_select = wordno & 0x3;
		int acc_shift = 16+(66 - (hmx_state->QDSP6_MX_FP_ACC_FRAC+hmx_state->QDSP6_MX_FP_ACC_INT)) + (word_select*32);
#ifdef HEX_CONFIG_INT128
		size16s_t acc = int128_zero();
		spatial_index <<= 1;
        acc = int128_make64((size8u_t)val);
#else
		size16s_t acc =  {0};
		spatial_index <<= 1;
 		acc.w[0] = val;
#endif
		acc = shiftl128(acc, acc_shift);
		if (word_select == 0) {
#ifdef HEX_CONFIG_INT128
			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select] =
                int128_zero();
#else
 			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[0] = 0;
 			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[1] = 0;
 			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[2] = 0;
 			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[3] = 0;
#endif
		}
#ifdef HEX_CONFIG_INT128
        hmx_state->accum_flt[spatial_index][channel_index].val[acc_select] = int128_or(
            hmx_state->accum_flt[spatial_index][channel_index].val[acc_select],
            acc);
#else
 		hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[0] |= acc.w[0];
 		hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[1] |= acc.w[1];
 		hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[2] |= acc.w[2];
 		hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].w[3] |= acc.w[3];
#endif

		// Sign extend
		hmx_state->accum_flt[spatial_index][channel_index].val[acc_select] = shiftr128(shiftl128(hmx_state->accum_flt[spatial_index][channel_index].val[acc_select], 46), 46);
		fMX_DEBUG_LOG(2,  "TB HMX WRITE: FLT ACC[%02d][%02d][%02d] = %016llx.%016llx w[%d]=%08x",
			spatial_index/2, channel_index, acc_select,
			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].hi,
			hmx_state->accum_flt[spatial_index][channel_index].val[acc_select].lo, wordno, val);
		return 0;
	}
	return -1;
}

int hmx_ext_get_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		uint32_t bias_flag = (hmx_state->accum_fxp[spatial_index][channel_index].bias_state & (1 << (wordno)))>0;
		uint32_t bias = bias_flag ? hmx_state->internal_bias_value : 0;
		*result = (size4u_t)(hmx_state->accum_fxp[spatial_index][channel_index].w[wordno] - bias);
		fMX_DEBUG_LOG(2, "TB HMX READ: FXP ACC[%02d][%02d][%02d]=%08x return unbias val=%08x (biased: %d subtracting bias val: %08x) bstate=%x pktid:%08x", spatial_index, channel_index, wordno, hmx_state->accum_fxp[spatial_index][channel_index].w[wordno], *result, bias_flag,  bias, THREAD2HMXSTRUCT->accum_fxp[spatial_index][channel_index].bias_state, thread->pktid);
		return 0;
	}
	return -1;
}
int hmx_ext_set_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		uint32_t is_biased = (wordno>>16);
		hmx_state->accum_fxp[spatial_index][channel_index].w[wordno & 0xFFFF] = (size4s_t)val + ((is_biased) ? hmx_state->internal_bias_value : 0);
		uint32_t bias_flag = (is_biased << (wordno & 0xFFFF));
		uint32_t old_state __attribute__((unused)) = hmx_state->accum_fxp[spatial_index][channel_index].bias_state;
		hmx_state->accum_fxp[spatial_index][channel_index].bias_state &= ~(1 << (wordno & 0xFFFF));	//clear flag
		hmx_state->accum_fxp[spatial_index][channel_index].bias_state |= bias_flag;
		fMX_DEBUG_LOG(1, "TB HMX WRITE: FXP ACC[%02d][%02d][%02d]=%08x (incoming val: %08x, bias_flag: %08x bstate old=%x new=%x)", spatial_index, channel_index, wordno & 0xFFFF, hmx_state->accum_fxp[spatial_index][channel_index].w[wordno & 0xFFFF], val, bias_flag, old_state, hmx_state->accum_fxp[spatial_index][channel_index].bias_state);
		return 0;
	}
	return -1;
}

int hmx_ext_get_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		*result = hmx_state->bias[arrayno][channel_index].val[wordno];
		fMX_DEBUG_LOG(2, "TB HMX READ: BIAS [%02d][%02d].w[%d] = %08x", arrayno, channel_index, wordno, *result);
		return 0;
	}
	return -1;
}

int hmx_ext_set_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2HMXSTRUCT != NULL) {
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		fMX_DEBUG_LOG(2, "TB HMX WRITE: BIAS [%02d][%02d].w[%d] = %08x", arrayno, channel_index, wordno, val);
		hmx_state->bias[arrayno][channel_index].val[wordno] = val;
		hmx_state->future_bias[arrayno][channel_index].val[wordno] = val;
		return 0;
	}
	return -1;
}

size4u_t hmx_ext_set_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index, size4u_t val) {
	if (THREAD2HMXSTRUCT != NULL){
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		hmx_state->cvt_accum[age ^ hmx_state->cvt_accum_current_index][spatial_idx][channel_idx].w[state_index] = val;
		if(age == 0){
			hmx_state->cvt_future_accum_fxp[spatial_idx][channel_idx].uh[0] = val;
			hmx_state->cvt_fallback_future_accum_fxp[spatial_idx][channel_idx].uh[0] = val;
			hmx_state->cvt_future_accum_flt[spatial_idx][channel_idx].uh[0] = val;
			hmx_state->cvt_fallback_future_accum_flt[spatial_idx][channel_idx].uh[0] = val;
		}
		fMX_DEBUG_LOG(2, "TB HMX WRITE: CVT ST[%02d][%02d] [%02d][%02d] = 0x%02x", age, state_index, spatial_idx, channel_idx, val);
		return 0;
	}
	return -1;
}

size4u_t hmx_ext_get_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index){
	if (THREAD2HMXSTRUCT != NULL){
		hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
		fMX_DEBUG_LOG(2, "TB HMX READ: CVT STATE[%02d][%02d] [%02d][%02d]", age, state_index, spatial_idx, channel_idx);
		return hmx_state->cvt_accum[age ^ hmx_state->cvt_accum_current_index][spatial_idx][channel_idx].w[state_index];
	}
	return -1;
}

void hmx_age_cvt_state(thread_t *thread){
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	hmx_state->cvt_accum_current_index = (hmx_state->cvt_accum_current_index + 1) % MAX_CONVERT_STATES;
}

int hmx_read_flt_acc_idx(thread_t *thread){
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	return hmx_state->current_acc_flt;
}

int hmx_read_fxp_acc_idx(thread_t *thread){
	hmx_state_t *hmx_state = THREAD2HMXSTRUCT;
	return hmx_state->current_acc_fxp;
}

/* Checkpoint save/restore functions; ignore SYSCFG vector length here */
void hmx_ext_dump_acc(FILE *fp, processor_t *proc, int extno);
void hmx_ext_dump_acc(FILE *fp, processor_t *proc, int extno) {


}

void hmx_ext_analyze_packet(thread_t * thread, Packet *pkt) {
#if 0
	processor_t *proc = thread->processor_ptr;
	if (!thread->bq_on) {
		INC_TSTATNPC(thmx_pkt_thread, (GET_SSR_FIELD(SSR_XE2) != 0), pkt->PC_VA);
	}
	if (pkt->pkt_has_shared_extension && !thread->bq_on) {
		for (int i = 0; i < pkt->num_insns; i++) {
			size2u_t opcode = pkt->insn[i].opcode;
			//int swap_inc = (opcode == M8_mxswap) || (opcode == M8_mxswap_hf) || (opcode == M8_mxaccshl);
			int clr_inc = (opcode == M8_mxclracc) || (opcode == M8_mxclracc_hf );
			INC_PSTATNPC(phmx_cvt, clr_inc , pkt->PC_VA);
			INC_PSTATNPC(phmxcvt_clr, clr_inc, pkt->PC_VA);

		}
		INC_TSTAT(thmx_pkt);

		INC_TSTATN(tpktstat_ext_insn1, ((pkt->num_insns-pkt->pkt_has_endloop)==1));
		INC_TSTATN(tpktstat_ext_insn2, ((pkt->num_insns-pkt->pkt_has_endloop)==2));
		INC_TSTATN(tpktstat_ext_insn3, ((pkt->num_insns-pkt->pkt_has_endloop)==3));
		INC_TSTATN(tpktstat_ext_insn4, ((pkt->num_insns-pkt->pkt_has_endloop)==4));

		INC_TSTATN(thmx_ld_cnt,  (pkt->pkt_hmx_ld_ct==1));
		INC_TSTATN(thmx_ld2_cnt, (pkt->pkt_hmx_ld_ct==2));
		INC_TSTATN(thmx_st_cnt,  (pkt->pkt_hmx_st_ct==1));

	} else if (!pkt->pkt_has_shared_extension && !thread->bq_on && GET_SSR_FIELD(SSR_XE2)) {

		INC_TSTATN(tpktstat_insn1, ((pkt->num_insns-pkt->pkt_has_endloop)==1));
		INC_TSTATN(tpktstat_insn2, ((pkt->num_insns-pkt->pkt_has_endloop)==2));
		INC_TSTATN(tpktstat_insn3, ((pkt->num_insns-pkt->pkt_has_endloop)==3));
		INC_TSTATN(tpktstat_insn4, ((pkt->num_insns-pkt->pkt_has_endloop)==4));

	}
#endif
}

