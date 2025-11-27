/*
 * Xqci manual helper function definitions.
 *
 * Copyright (c) 2025 rev.ng Labs Srl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.
 *
 * See the LICENSE file in the top-level directory for details.
 */

#include "system/memory.h"
#include "system/address-spaces.h"

#ifdef TARGET_RISCV32
void HELPER(xqci_swm)(CPUArchState *env, target_ulong vaddr, target_ulong length, int rs3)
{
    int mmu_idx = riscv_env_mmu_index(env, false);

    if (rs3 + length > 32) {
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }

    for (int i = 0; i < length; ++i) {
        cpu_stl_le_mmuidx_ra(env, vaddr, env->gpr[rs3 + i], mmu_idx, GETPC());
        vaddr += 4;
    }
}

void HELPER(xqci_lwm)(CPUArchState *env, target_ulong vaddr, target_ulong length, int rd)
{
    int mmu_idx = riscv_env_mmu_index(env, false);

    if (rd + length > 32) {
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }

    for (int i = 0; i < length; ++i) {
        env->gpr[rd + i] = cpu_ldl_le_mmuidx_ra(env, vaddr, mmu_idx, GETPC());
        vaddr += 4;
    }
}

void HELPER(xqci_setwm)(CPUArchState *env, target_ulong vaddr, target_ulong length, target_ulong value)
{
    int mmu_idx = riscv_env_mmu_index(env, false);

    for (int i = 0; i < length; ++i) {
        cpu_stl_le_mmuidx_ra(env, vaddr, value, mmu_idx, GETPC());
        vaddr += 4;
    }
}

#ifndef CONFIG_USER_ONLY
static void xqci_set_mode(CPUArchState *env, int mode)
{
#ifndef CONFIG_USER_ONLY
    riscv_cpu_set_mode(env, mode, env->virt_enabled);
#else
    env->priv = mode;
#endif
}

static void mienter_copy_cpustate(CPUArchState *env, uintptr_t ra)
{
    /* TODO(anjo): validate env->gpr[2] */
    target_ulong vaddr = env->gpr[2];
    target_ulong mepc;
    target_ulong mnepc;
    target_ulong qc_mcause;
    target_ulong reserved_val = 0;
    int mmu_idx = riscv_env_mmu_index(env, false);
    target_ulong mask = (1ull << 32) - 1;
    RISCVException ret;

    ret = riscv_csrr(env, CSR_MEPC, &mepc);
    if (ret != RISCV_EXCP_NONE) {
        riscv_raise_exception(env, ret, ra);
    }

    ret = riscv_csrr(env, CSR_MNEPC, &mnepc);
    if (ret != RISCV_EXCP_NONE) {
        riscv_raise_exception(env, ret, ra);
    }

    ret = riscv_csrr(env, CSR_MCAUSE, &qc_mcause);
    if (ret != RISCV_EXCP_NONE) {
        riscv_raise_exception(env, ret, ra);
    }

    if (get_field(env->mnstatus, MNSTATUS_NMIE) == 1) {
        cpu_stl_le_mmuidx_ra(env, vaddr - 4, mepc, mmu_idx, ra);
    } else {
        cpu_stl_le_mmuidx_ra(env, vaddr - 4, mnepc, mmu_idx, ra);
    }

    cpu_stl_le_mmuidx_ra(env, vaddr - 8,  env->gpr[8] & mask,  mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 12, qc_mcause,           mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 16, env->gpr[1] & mask,  mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 20, reserved_val,        mmu_idx, ra);

    cpu_stl_le_mmuidx_ra(env, vaddr - 24, env->gpr[5] & mask,  mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 28, env->gpr[6] & mask,  mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 32, env->gpr[7] & mask,  mmu_idx, ra);

    cpu_stl_le_mmuidx_ra(env, vaddr - 36, env->gpr[10] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 40, env->gpr[11] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 44, env->gpr[12] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 48, env->gpr[13] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 52, env->gpr[14] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 56, env->gpr[15] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 60, env->gpr[16] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 64, env->gpr[17] & mask, mmu_idx, ra);

    cpu_stl_le_mmuidx_ra(env, vaddr - 68, env->gpr[28] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 72, env->gpr[29] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 76, env->gpr[30] & mask, mmu_idx, ra);
    cpu_stl_le_mmuidx_ra(env, vaddr - 80, env->gpr[31] & mask, mmu_idx, ra);

    env->gpr[8] = env->gpr[2];
    env->gpr[2] -= 96;
}

void HELPER(xqci_mienter)(CPUArchState *env)
{
    mienter_copy_cpustate(env, GETPC());
}

void HELPER(xqci_mienter_nest)(CPUArchState *env)
{
    target_ulong qc_mcause_val = env->qc_mcause;
    mienter_copy_cpustate(env, GETPC());
    env->mstatus = set_field(env->mstatus, MSTATUS_MIE, 1);
    env->qc_mcause = qc_mcause_val | (1 << 26);
}

void HELPER(xqci_mileaveret)(CPUArchState *env)
{
    /* TODO(anjo): validate env->gpr[2] */
    target_ulong vaddr = env->gpr[2] + 96;
    target_ulong prev_retpc;
    target_ulong qc_mcause_val;
    int nmie_val = get_field(env->mnstatus, MNSTATUS_NMIE);

    int mmu_idx = riscv_env_mmu_index(env, false);

    prev_retpc = cpu_ldl_le_mmuidx_ra(env, vaddr - 4, mmu_idx, GETPC());
    (void)prev_retpc;
    qc_mcause_val = cpu_ldl_le_mmuidx_ra(env, vaddr - 12, mmu_idx, GETPC());

    env->gpr[8] = cpu_ldl_le_mmuidx_ra(env, vaddr - 8, mmu_idx, GETPC());
    env->qc_mcause = qc_mcause_val;
    env->gpr[1] = cpu_ldl_le_mmuidx_ra(env, vaddr - 16, mmu_idx, GETPC());

    env->gpr[5] = cpu_ldl_le_mmuidx_ra(env, vaddr - 24, mmu_idx, GETPC());
    env->gpr[6] = cpu_ldl_le_mmuidx_ra(env, vaddr - 28, mmu_idx, GETPC());
    env->gpr[7] = cpu_ldl_le_mmuidx_ra(env, vaddr - 32, mmu_idx, GETPC());

    env->gpr[10] = cpu_ldl_le_mmuidx_ra(env, vaddr - 36, mmu_idx, GETPC());
    env->gpr[11] = cpu_ldl_le_mmuidx_ra(env, vaddr - 40, mmu_idx, GETPC());
    env->gpr[12] = cpu_ldl_le_mmuidx_ra(env, vaddr - 44, mmu_idx, GETPC());
    env->gpr[13] = cpu_ldl_le_mmuidx_ra(env, vaddr - 48, mmu_idx, GETPC());
    env->gpr[14] = cpu_ldl_le_mmuidx_ra(env, vaddr - 52, mmu_idx, GETPC());
    env->gpr[15] = cpu_ldl_le_mmuidx_ra(env, vaddr - 56, mmu_idx, GETPC());
    env->gpr[16] = cpu_ldl_le_mmuidx_ra(env, vaddr - 60, mmu_idx, GETPC());
    env->gpr[17] = cpu_ldl_le_mmuidx_ra(env, vaddr - 64, mmu_idx, GETPC());

    env->gpr[28] = cpu_ldl_le_mmuidx_ra(env, vaddr - 68, mmu_idx, GETPC());
    env->gpr[29] = cpu_ldl_le_mmuidx_ra(env, vaddr - 72, mmu_idx, GETPC());
    env->gpr[30] = cpu_ldl_le_mmuidx_ra(env, vaddr - 76, mmu_idx, GETPC());
    env->gpr[31] = cpu_ldl_le_mmuidx_ra(env, vaddr - 80, mmu_idx, GETPC());

    env->gpr[2] += 96;

    if (nmie_val == 1) {
        target_ulong qc_mcause_val_masked = qc_mcause_val & ~(1<<26) & ~(1<<27) & ~(1<<29) & ~(0xFF<<12);
        int mpie_val = (qc_mcause_val >> 27) & 1;
        int mpdt_val = (qc_mcause_val >> 29) & 1;
        int mpil_val = (qc_mcause_val >> 16) & 0xF;
        env->mstatus = set_field(env->mstatus, MSTATUS_MIE, mpie_val);
        env->mstatus = set_field(env->mstatus, MSTATUS_MPIE, 1);
        /* Default rv32 implements Smdbltrp */
        /* if (implemented?(ExtensionName::Smdbltrp)) {*/
        target_ulong mstatush;
        riscv_csrr(env, CSR_MSTATUSH, &mstatush);
        mstatush = set_field(mstatush, MSTATUSH_MDT, mpdt_val);
        riscv_csrrw(env, CSR_MSTATUSH, NULL, mstatush, UINT32_MAX, GETPC());
        /* } */
        env->qc_mcause = (qc_mcause_val_masked |
            (1<<27) | (mpie_val<<26) | (0<<29) |
            (mpil_val << 12) | (0xF << 16));
        if (mpdt_val == 0) {
            int mpp = get_field(env->mstatus, MSTATUS_MPP);
            if (mpp != 0b11) {
                env->mstatus = set_field(env->mstatus, MSTATUS_MPRV, 0);
            }
            if (mpp == 0b00) {
                xqci_set_mode(env, PRV_U);
            } else if (mpp == 0b01) {
                xqci_set_mode(env, PRV_S);
            } else if (mpp == 0b11) {
                xqci_set_mode(env, PRV_M);
            }
            env->mstatus = set_field(env->mstatus, MSTATUS_MPP, (riscv_has_ext(env, RVU)) ? 0b00 : 0b11);
        }
        env->pc = env->mepc;
    } else {
        target_ulong qc_mcause_val_masked = qc_mcause_val & ~(1<<26) & ~(1<<28) & ~(1<<30) & ~(0xF<<12) & ~(0xF<<20);
        int mnpie_val = (qc_mcause_val >> 28) & 1;
        int mnpil_val = (qc_mcause_val >> 20) & 0xF;
        env->mstatus = set_field(env->mstatus, MSTATUS_MIE, mnpie_val);
        env->mnstatus = set_field(env->mnstatus, MNSTATUS_NMIE, 1);
        env->qc_mcause = (qc_mcause_val_masked |
            (1<<28) | (mnpie_val<<26) | (1<<30) |
            (mnpil_val << 12) | (0xF << 20));
        int mnpp = get_field(env->mnstatus, MNSTATUS_MNPP);
        if (mnpp != 0b11) {
            env->mstatus = set_field(env->mstatus, MSTATUS_MPRV, 0);
            /* Default rv32 implements Smdbltrp */
            /* if (implemented?(ExtensionName::Smdbltrp)) {*/
            riscv_csrrw(env, CSR_MSTATUSH, NULL, 0, MSTATUSH_MDT, GETPC());
            /* } */
        }
        if (mnpp == 0b00) {
            xqci_set_mode(env, PRV_U);
        } else if (mnpp == 0b01) {
            xqci_set_mode(env, PRV_S);
        } else if (mnpp == 0b11) {
            xqci_set_mode(env, PRV_M);
        }
        env->mnstatus = set_field(env->mnstatus, MNSTATUS_MNPP, (riscv_has_ext(env, RVU)) ? 0b00 : 0b11);
        env->pc = env->mnepc;
    }
}

static void emit_qc_c_mienter(DisasContext *ctx, TCGv_env tcg_env)
{
    gen_helper_xqci_mienter(tcg_env);
}

static void emit_qc_c_mienter_nest(DisasContext *ctx, TCGv_env tcg_env)
{
    gen_helper_xqci_mienter_nest(tcg_env);
}

static void emit_qc_c_mileaveret(DisasContext *ctx, TCGv_env tcg_env)
{
    gen_helper_xqci_mileaveret(tcg_env);
}
#endif /* !CONFIG_USER_ONLY */

static TCGv xqci_ea(DisasContext *ctx, int rs1, int imm)
{
    TCGv vaddr = tcg_temp_new();
    tcg_gen_addi_tl(vaddr, get_gpr(ctx, rs1, EXT_NONE), imm);
    return vaddr;
}

static void emit_qc_brev32(DisasContext *ctx, TCGv_env tcg_env, int rs1, int rd)
{
    TCGv dst = dest_gpr(ctx, rd);
    TCGv src = get_gpr(ctx, rs1, EXT_NONE);
    gen_helper_brev32(dst, src);
}

static void emit_qc_swm(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int rs2, int rs3)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    TCGv length = get_gpr(ctx, rs2, EXT_NONE);
    gen_helper_xqci_swm(tcg_env, vaddr, length, tcg_constant_i32(rs3));
}

static void emit_qc_swmi(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int length, int rs3)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    gen_helper_xqci_swm(tcg_env, vaddr, tcg_constant_tl(length), tcg_constant_i32(rs3));
}

static void emit_qc_lwm(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int rs2, int rd)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    TCGv length = get_gpr(ctx, rs2, EXT_NONE);
    gen_helper_xqci_lwm(tcg_env, vaddr, length, tcg_constant_i32(rd));
}

static void emit_qc_lwmi(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int length, int rd)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    gen_helper_xqci_lwm(tcg_env, vaddr, tcg_constant_tl(length), tcg_constant_i32(rd));
}

static void emit_qc_setwm(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int rs2, int rs3)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    TCGv length = get_gpr(ctx, rs2, EXT_NONE);
    TCGv value = get_gpr(ctx, rs3, EXT_NONE);
    gen_helper_xqci_setwm(tcg_env, vaddr, length, value);
}

static void emit_qc_setwmi(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int length, int rs3)
{
    TCGv vaddr = xqci_ea(ctx, rs1, imm);
    TCGv value = get_gpr(ctx, rs3, EXT_NONE);
    gen_helper_xqci_setwm(tcg_env, vaddr, tcg_constant_tl(length), value);
}


#ifndef CONFIG_USER_ONLY
/*
uint8_t sync_bitmask(int slist)
{
    Bits<5> bitmask;
    if (slist == 0) {
      bitmask = 0;
    } else if (slist < 6) {
      XReg shift = slist - 1;
      bitmask = (1 << shift);
    } else {
      XReg shift = slist - 2;
      bitmask = (1 << shift) - 1;
    }
    return 0;
}
*/

static void emit_qc_c_sync(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    /* uint8_t bitmask = sync_bitmask(slist); */
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_read_after_write_device(true,bitmask); */
}

static void emit_qc_c_syncr(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    /* uint8_t bitmask = sync_bitmask(slist); */
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_read_after_write_device(false,bitmask); */
}

static void emit_qc_c_syncwf(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    /* uint8_t bitmask = sync_bitmask(slist); */
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_write_after_read_device(false,bitmask); */
}

static void emit_qc_c_syncwl(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    /* uint8_t bitmask = sync_bitmask(slist); */
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_write_after_read_device(true,bitmask); */
}

static void emit_qc_sync(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_read_after_write_device(true,imm); */
}

static void emit_qc_syncr(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_read_after_write_device(false,imm); */
}

static void emit_qc_syncwf(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_write_after_read_device(false,imm); */
}

static void emit_qc_syncwl(DisasContext *ctx, TCGv_env tcg_env, int slist)
{
    tcg_gen_mb(TCG_MO_ALL);
    /* sync_write_after_read_device(true,imm); */
}

void HELPER(xqci_outw)(CPUArchState *env, target_ulong address, target_ulong value)
{
#ifndef CONFIG_USER_ONLY
    address_space_stl(&address_space_io, address, value,
                      MEMTXATTRS_UNSPECIFIED, NULL);
#endif
}

target_ulong HELPER(xqci_inw)(CPUArchState *env, target_ulong address)
{
#ifndef CONFIG_USER_ONLY
    return address_space_ldl(&address_space_io, address,
                             MEMTXATTRS_UNSPECIFIED, NULL);
#else
    return 0;
#endif
}

static void emit_qc_inw(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int rd)
{
    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, get_gpr(ctx, rs1, EXT_NONE), imm);
    TCGv dst = dest_gpr(ctx, rd);
    translator_io_start(&ctx->base);
    gen_helper_xqci_inw(dst, tcg_env, addr);
}

static void emit_qc_outw(DisasContext *ctx, TCGv_env tcg_env, int imm, int rs1, int rs2)
{
    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, get_gpr(ctx, rs1, EXT_NONE), imm);
    TCGv value = get_gpr(ctx, rs2, EXT_NONE);
    translator_io_start(&ctx->base);
    gen_helper_xqci_outw(tcg_env, addr, value);
}
#endif /* !CONFIG_USER_ONLY */

static void emit_qc_csrrwr(DisasContext *ctx, TCGv_env tcg_env, int rs2, int rs1, int rd)
{
    TCGv csr = get_gpr(ctx, rs2, EXT_NONE);
    if (rd != 0) {
        gen_helper_csrr(dest_gpr(ctx, rd), tcg_env, csr);
    }
    gen_helper_csrw(tcg_env, csr, get_gpr(ctx, rs1, EXT_NONE));
}

static void emit_qc_csrrwri(DisasContext *ctx, TCGv_env tcg_env, int rs2, int imm, int rd)
{
    TCGv csr = get_gpr(ctx, rs2, EXT_NONE);
    if (rd != 0) {
        gen_helper_csrr(dest_gpr(ctx, rd), tcg_env, csr);
    }
    gen_helper_csrw(tcg_env, csr, tcg_constant_tl((uint32_t)imm));
}

#endif
