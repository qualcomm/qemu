/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "hex_regs.h"
#include "reg_fields.h"

static inline void gen_vmsetregs(void)
{
    tcg_gen_mov_tl(hex_g_sreg[HEX_GREG_G0], hex_gpr[HEX_REG_R00]);
    tcg_gen_mov_tl(hex_g_sreg[HEX_GREG_G1], hex_gpr[HEX_REG_R01]);
    tcg_gen_mov_tl(hex_g_sreg[HEX_GREG_G2], hex_gpr[HEX_REG_R02]);
    tcg_gen_mov_tl(hex_g_sreg[HEX_GREG_G3], hex_gpr[HEX_REG_R03]);
}

static inline void gen_vmgetregs(void)
{
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R00], hex_g_sreg[HEX_GREG_G0]);
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R01], hex_g_sreg[HEX_GREG_G1]);
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R02], hex_g_sreg[HEX_GREG_G2]);
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R03], hex_g_sreg[HEX_GREG_G3]);
}

static inline void gen_vmsetie(void)
{
    TCGv old_ie = tcg_temp_new();
    tcg_gen_extract_tl(old_ie, hex_g_sreg[HEX_SREG_SYSCFG],
                       reg_field_info[SYSCFG_GIE].offset,
                       reg_field_info[SYSCFG_GIE].width);

    TCGv new_syscfg = tcg_temp_new();
    tcg_gen_deposit_tl(new_syscfg, hex_g_sreg[HEX_SREG_SYSCFG],
                       hex_gpr[HEX_REG_R00],
                       reg_field_info[SYSCFG_GIE].offset,
                       reg_field_info[SYSCFG_GIE].width);
    gen_helper_modify_syscfg(tcg_env, new_syscfg);
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R00], old_ie);
}

static inline void gen_vmgetie(void)
{
    tcg_gen_extract_tl(hex_gpr[HEX_REG_R00], hex_g_sreg[HEX_SREG_SYSCFG],
                       reg_field_info[SYSCFG_GIE].offset,
                       reg_field_info[SYSCFG_GIE].width);
}

static void gen_vmvpid(void)
{
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R00], hex_t_sreg[HEX_SREG_HTID]);
}

static void gen_vmgettime(void)
{
    /*
     * For now we'll take only the cycles from this vCPU, which seems
     * reasonable for this use case.
     */
    tcg_gen_extr_i64_i32(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R01],
                         hex_cycle_count);
}

static void gen_vmsettime(void) {
  /*
   * Set the cycle count from the 64-bit value in R0:R1.
   * This is the reverse of gen_vmgettime().
   */
  tcg_gen_concat_i32_i64(hex_cycle_count, hex_gpr[HEX_REG_R00],
                         hex_gpr[HEX_REG_R01]);
}

static void gen_vmcache(void)
{
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
}

static inline void gen_vmversion(void)
{
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0x800);
}

static inline void gen_vmstop(void)
{
    gen_helper_stop(tcg_env);
}
static inline void gen_vmstart(void)
{
    g_assert_not_reached();
}
static inline void gen_vmyield(void)
{
    /* gen_exception(EXCP_YIELD, ctx->next_PC); ?? */
}
static inline void gen_vmnewmap(void)
{
  /* vmnewmap calling convention:
   * R0 = input parameter (address with type info encoded in lower bits)
   * R1 = secondary parameter (unused in current implementation)
   * R2 = tlb invalidation mask
   *
   * For page table mapping, map_type is always 1, and the type index
   * is extracted from the lower bits of R0 in the helper function.
   */
  TCGv map_type = tcg_constant_tl(1); /* Always use page table mapping */
  gen_helper_vmnewmap(tcg_env, map_type, hex_gpr[HEX_REG_R00],
                      hex_gpr[HEX_REG_R02]);
}
static inline void gen_vmclrmap(void)
{
    g_assert_not_reached();
}

typedef enum VmIntOp {
    VMINTOP_NOP = 0,
    VMINTOP_GLOBEN = 1,
    VMINTOP_GLOBDIS = 2,
    VMINTOP_LOCEN = 3,
    VMINTOP_LOCDIS = 4,
    VMINTOP_AFFINITY = 5,
    VMINTOP_GET = 6,
    VMINTOP_PEEK = 7,
    VMINTOP_STATUS = 8,
    VMINTOP_POST = 9,
    VMINTOP_CLEAR = 10,
} VmIntOp;

static inline void gen_vmintop(void)
{
    TCGLabel *global_en = gen_new_label();
    TCGLabel *global_dis = gen_new_label();
    TCGLabel *local_en = gen_new_label();
    TCGLabel *local_dis = gen_new_label();
    TCGLabel *affinity = gen_new_label();
    TCGLabel *get = gen_new_label();
    TCGLabel *peek = gen_new_label();
    TCGLabel *status = gen_new_label();
    TCGLabel *post = gen_new_label();
    TCGLabel *clear = gen_new_label();
    TCGLabel *exit_ = gen_new_label();

    TCGv disable_irq = tcg_temp_new();
    tcg_gen_movi_tl(disable_irq, false);

    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_GLOBEN,
                       global_en);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_GLOBDIS,
                       global_dis);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_LOCEN,
                       local_en);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_LOCDIS,
                       local_dis);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_GET,
                       get);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_AFFINITY,
                       affinity);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_PEEK,
                       peek);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_STATUS,
                       status);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_POST,
                       post);
    tcg_gen_brcondi_tl(TCG_COND_EQ, hex_gpr[HEX_REG_R00], VMINTOP_CLEAR,
                       clear);

    TCGv irq_mask = tcg_temp_new();
    TCGv set_mask = tcg_temp_new();
    TCGv clear_mask = tcg_temp_new();

    gen_set_label(global_en);
    tcg_gen_shl_tl(irq_mask, tcg_constant_tl(1), hex_gpr[HEX_REG_R01]);
    gen_helper_fbrev(irq_mask, irq_mask);
    gen_helper_ciad(tcg_env, irq_mask);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
    tcg_gen_br(exit_);

    gen_set_label(global_dis);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], -1);
    tcg_gen_br(exit_);

    gen_set_label(local_dis);
    tcg_gen_movi_tl(disable_irq, true);
    gen_set_label(local_en);

    /*
     * set_mask = 1 << r1
     * clear_mask = brev(imask) & ~set_mask
     * set_mask = brev(imask) | set_mask
     * imask = brev(disable_irq ? set_mask : clear_mask)
     */
    tcg_gen_shl_tl(set_mask, tcg_constant_tl(1), hex_gpr[HEX_REG_R01]);
    gen_helper_fbrev(irq_mask, hex_t_sreg[HEX_SREG_IMASK]);
    tcg_gen_neg_tl(clear_mask, set_mask);
    tcg_gen_and_tl(clear_mask, irq_mask, clear_mask);
    tcg_gen_or_tl(set_mask, irq_mask, set_mask);
    tcg_gen_movcond_tl(TCG_COND_EQ, irq_mask, disable_irq,
                       tcg_constant_tl(true), set_mask, clear_mask);
    gen_helper_fbrev(hex_t_sreg[HEX_SREG_IMASK], irq_mask);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
    tcg_gen_br(exit_);

    gen_set_label(affinity);
    tcg_gen_shl_tl(clear_mask, tcg_constant_tl(1), hex_gpr[HEX_REG_R02]);
    tcg_gen_neg_tl(clear_mask, clear_mask);
    /*
     * FIXME do we need `r10 = combine(r1.l,r10.l)` ?
     */
    gen_helper_iassignw(tcg_env, clear_mask);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
    tcg_gen_br(exit_);

    gen_set_label(get);
    /* FIXME implement */
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], -1);
    tcg_gen_br(exit_);

    gen_set_label(peek);
    TCGv ipend = tcg_temp_new();
    TCGv high_irq = tcg_temp_new();
    tcg_gen_extract_tl(ipend, hex_g_sreg[HEX_SREG_IPENDAD],
                       reg_field_info[IPENDAD_IPEND].offset,
                       reg_field_info[IPENDAD_IPEND].width);
    tcg_gen_clzi_tl(high_irq, ipend, 32); /* FIXME is that right? */
    const int MAX_IRQ = reg_field_info[IPENDAD_IPEND].width;
    tcg_gen_movcond_tl(TCG_COND_GE, hex_gpr[HEX_REG_R00], high_irq,
                       tcg_constant_tl(MAX_IRQ), tcg_constant_tl(-1), high_irq);
    tcg_gen_br(exit_);

    gen_set_label(status);
    /*
     * result: bitmask, bit 0: pending for IRQ # (IPEND)
     *                  bit 1: locally enabled for IRQ # (~IAD)
     *                  bit 2: globally enabled for IRQ # (~IMASK)
     *         failure: -1 (all 32 bits set)
     *
     * r0 = (cpuint_pending >> irq) & 1;
     * r0 |= (((cpuint_enabled >> irq) & 1) << 2);
     *  ^^ but what about bit #2?
     */
    TCGv iad = tcg_temp_new();
    tcg_gen_extract_tl(iad, hex_g_sreg[HEX_SREG_IPENDAD],
                       reg_field_info[IPENDAD_IAD].offset,
                       reg_field_info[IPENDAD_IAD].width);
    tcg_gen_extract_tl(ipend, hex_g_sreg[HEX_SREG_IPENDAD],
                       reg_field_info[IPENDAD_IPEND].offset,
                       reg_field_info[IPENDAD_IPEND].width);
    TCGv tmp = tcg_temp_new();

    tcg_gen_shr_tl(tmp, ipend, hex_gpr[HEX_REG_R01]);
    tcg_gen_andi_tl(tmp, tmp, 0x1);

    tcg_gen_shr_tl(hex_gpr[HEX_REG_R00], iad, hex_gpr[HEX_REG_R01]);
    tcg_gen_andi_tl(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R00], 0x1);
    tcg_gen_shli_tl(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R00], 1);

    tcg_gen_or_tl(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R00], tmp);

    tcg_gen_shr_tl(tmp, hex_t_sreg[HEX_SREG_IMASK], hex_gpr[HEX_REG_R01]);
    tcg_gen_andi_tl(tmp, tmp, 0x1);
    tcg_gen_shli_tl(tmp, tmp, 2);

    /* Invert the sense of disabled, masked bits: 0b0110 */
    tcg_gen_or_tl(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R00], tmp);
    tcg_gen_xori_tl(hex_gpr[HEX_REG_R00], hex_gpr[HEX_REG_R00], 0x06);
    tcg_gen_br(exit_);

    gen_set_label(post);
    tcg_gen_shl_tl(irq_mask, tcg_constant_tl(1), hex_gpr[HEX_REG_R01]);
    gen_helper_fbrev(irq_mask, irq_mask);
    gen_helper_swi(tcg_env, irq_mask);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
    tcg_gen_br(exit_);

    gen_set_label(clear);
    tcg_gen_shl_tl(irq_mask, tcg_constant_tl(1), hex_gpr[HEX_REG_R01]);
    gen_helper_fbrev(irq_mask, irq_mask);
    gen_helper_cswi(tcg_env, irq_mask);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 1); /* FIXME why 1? */
    tcg_gen_br(exit_);

    gen_set_label(exit_);
}

static inline void gen_vmsetvec(void)
{
    tcg_gen_mov_tl(hex_t_sreg[HEX_SREG_GEVB], hex_gpr[HEX_REG_R00]);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
}
static inline void gen_vmrte(void)
{
    TCGv gsr_ie = tcg_temp_new();
    /* SYSCFG.GIE = GSR.IE */
    tcg_gen_extract_tl(gsr_ie, hex_t_sreg[HEX_GREG_GSR],
                       reg_field_info[GSR_IE].offset,
                       reg_field_info[GSR_IE].width);
    tcg_gen_deposit_tl(hex_g_sreg[HEX_SREG_SYSCFG], hex_g_sreg[HEX_SREG_SYSCFG],
                       gsr_ie,
                       reg_field_info[SYSCFG_GIE].offset,
                       reg_field_info[SYSCFG_GIE].width);

    TCGv gsr_um = tcg_temp_new();
    tcg_gen_extract_tl(gsr_um, hex_t_sreg[HEX_GREG_GSR],
                       reg_field_info[GSR_UM].offset,
                       reg_field_info[GSR_UM].width);
    TCGLabel *is_not_user_mode = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, gsr_um, 0, is_not_user_mode);

    /* if (GSR.UM) { Swap GOSP, r29 } */
    TCGv tmp = tcg_temp_new();
    tcg_gen_mov_tl(tmp, hex_t_sreg[HEX_GREG_GOSP]);
    tcg_gen_mov_tl(hex_t_sreg[HEX_GREG_GOSP], hex_gpr[HEX_REG_R29]);
    tcg_gen_mov_tl(hex_gpr[HEX_REG_R29], tmp);

    gen_set_label(is_not_user_mode);

    tcg_gen_mov_tl(hex_gpr[HEX_REG_PC], hex_t_sreg[HEX_GREG_GELR]);
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 0);
}

static inline void gen_vmgetinfo(void)
{
    gen_helper_vmgetinfo(hex_gpr[HEX_REG_R00], tcg_env, hex_gpr[HEX_REG_R00]);
}
static inline void gen_vmtimerop(void)
{
    /* FIXME */
    tcg_gen_movi_tl(hex_gpr[HEX_REG_R00], 1);
}
