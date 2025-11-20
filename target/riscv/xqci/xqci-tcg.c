#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "tcg/tcg-op.h"
#include "tcg/tcg-op-gvec.h"
#include "tcg/tcg.h"
#include "exec/helper-gen.h"

// void _ZN12CPUArchState9qc_addsatEhhh
static void emit_qc_addsat(DisasContext *ctx, TCGv_env env, uint8_t vi_24, uint8_t vi_21, uint8_t vi_4) {
TCGv_i64 temp3 = tcg_temp_new_i64();
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_24);
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_21);
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp9);
TCGv_i64 temp15 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp15, temp18);
tcg_gen_add_i64(temp6, temp15, temp6);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp9, 31);
TCGv_i32 temp13 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp13, temp18, 31);
tcg_gen_mov_i64(temp3, temp6);
TCGLabel * label28 = gen_new_label();
TCGLabel * label29 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_NE, temp13, temp0, label28);
gen_set_label(label29);
tcg_gen_extract_i32(temp13, temp0, 0, 8);
tcg_gen_shri_i64(temp15, temp6, 31ull);
TCGv_i64 temp34 = tcg_temp_new_i64();
tcg_gen_extract_i64(temp34, temp15, 0, 8);
tcg_gen_extrl_i64_i32(temp0, temp34);
tcg_gen_andi_i32(temp0, temp0, 1);
tcg_gen_mov_i64(temp3, temp6);
TCGLabel * label35 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp0, temp13, label28);
gen_set_label(label35);
tcg_gen_setcondi_i32(TCG_COND_LT, temp0, temp9, 0);
TCGv_i64 temp37 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp37, temp0);
tcg_gen_movcond_i64(TCG_COND_NE, temp6, temp37, tcg_constant_i64(0), tcg_constant_i64(6442450944ull), tcg_constant_i64(2147483647ull));
tcg_gen_mov_i64(temp3, temp6);
tcg_gen_br(label28);
gen_set_label(label28);
tcg_gen_mov_i64(temp3, temp3);
tcg_gen_extrl_i64_i32(temp0, temp3);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_addusatEhhh
static void emit_qc_addusat(DisasContext *ctx, TCGv_env env, uint8_t vi_14, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_11);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp9);
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp8);
tcg_gen_add_i64(temp3, temp6, temp3);
tcg_gen_umin_i64(temp6, temp3, tcg_constant_i64(4294967295ull));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_extrl_i64_i32(temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_beqiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_beqi(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp8, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_10 << 3)) >> 3)), label21);
gen_set_label(label22);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label21);
gen_set_label(label21);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState7qc_bgeiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_bgei(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LT, temp8, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_10 << 3)) >> 3)), label21);
gen_set_label(label22);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label21);
gen_set_label(label21);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState8qc_bgeuiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_bgeui(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label18 = gen_new_label();
TCGLabel * label19 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LTU, temp8, (vi_10 & 31), label18);
gen_set_label(label19);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label18);
gen_set_label(label18);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState7qc_bltiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_blti(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GE, temp8, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_10 << 3)) >> 3)), label21);
gen_set_label(label22);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label21);
gen_set_label(label21);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState8qc_bltuiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_bltui(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label18 = gen_new_label();
TCGLabel * label19 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GEU, temp8, (vi_10 & 31), label18);
gen_set_label(label19);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label18);
gen_set_label(label18);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState7qc_bneiE4BitsILj13ELb0EES0_ILj5ELb0EEh
static void emit_qc_bnei(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint8_t vi_10, uint8_t vi_14) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp8, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_10 << 3)) >> 3)), label21);
gen_set_label(label22);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label21);
gen_set_label(label21);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState10qc_c_bextiE4BitsILj5ELb0EEh
static void emit_qc_c_bexti(DisasContext *ctx, TCGv_env env, uint8_t vi_13, uint8_t vi_10) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, ((vi_10 & 7) | 8));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp6, (vi_13 & 31));
tcg_gen_andi_i32(temp0, temp0, 1);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) ((vi_10 & 7) | 8))], temp0);
}

// void _ZN12CPUArchState10qc_c_bsetiE4BitsILj5ELb0EEh
static void emit_qc_c_bseti(DisasContext *ctx, TCGv_env env, uint8_t vi_13, uint8_t vi_10) {
TCGv_i32 temp5 = xqci_get_gpr(ctx, ((vi_10 & 7) | 8));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp0, temp5, (1 << (vi_13 & 31)));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) ((vi_10 & 7) | 8))], temp0);
}

// void _ZN12CPUArchState11qc_c_clrintE4BitsILj5ELb0EE
static void emit_qc_c_clrint(DisasContext *ctx, TCGv_env env, uint8_t vi_7) {
TCGv_i32 temp3 = xqci_csrr(ctx, env, 2032);
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp2, temp3, ((1 << (vi_7 & 31)) ^ -1));
xqci_csrw(ctx, env, 2032, temp2);
}

// void _ZN12CPUArchState10qc_c_delayE4BitsILj5ELb0EE
static void emit_qc_c_delay(DisasContext *ctx, TCGv_env env, uint8_t vi_1) {
}

// void _ZN12CPUArchState7qc_c_diEv
static void emit_qc_c_di(DisasContext *ctx, TCGv_env env) {
xqci_csrw_field(ctx, env, 768, 8, tcg_constant_i32(0));
TCGv_i32 temp3 = xqci_csrr(ctx, env, 1993);
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp2, temp3, -67108865);
xqci_csrw(ctx, env, 1993, temp2);
}

// void _ZN12CPUArchState8qc_c_dirEh
static void emit_qc_c_dir(DisasContext *ctx, TCGv_env env, uint8_t vi_4) {
TCGv_i32 temp0 = xqci_csrr(ctx, env, 768);
xqci_csrw_field(ctx, env, 768, 8, tcg_constant_i32(0));
TCGv_i32 temp9 = xqci_csrr(ctx, env, 1993);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp9, -67108865);
xqci_csrw(ctx, env, 1993, temp8);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_c_eiEv
static void emit_qc_c_ei(DisasContext *ctx, TCGv_env env) {
xqci_csrw_field(ctx, env, 768, 8, tcg_constant_i32(1));
TCGv_i32 temp3 = xqci_csrr(ctx, env, 1993);
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp2, temp3, 67108864);
xqci_csrw(ctx, env, 1993, temp2);
}

// void _ZN12CPUArchState8qc_c_eirEh
static void emit_qc_c_eir(DisasContext *ctx, TCGv_env env, uint8_t vi_11) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp3 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp3, temp8, 3);
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp2, temp3, 1);
xqci_csrw_field(ctx, env, 768, 8, temp2);
temp3 = xqci_csrr(ctx, env, 1993);
tcg_gen_shli_i32(temp2, temp2, 26);
tcg_gen_or_i32(temp2, temp2, temp3);
xqci_csrw(ctx, env, 1993, temp2);
}

// void _ZN12CPUArchState9qc_c_extuE4BitsILj5ELb0EEh
static void emit_qc_c_extu(DisasContext *ctx, TCGv_env env, uint8_t vi_13, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp0, (((vi_13 & 31) == 31) ? tcg_constant_i32(-1) : tcg_constant_i32(((-1 << ((vi_13 & 31) + 1)) ^ -1))));
tcg_gen_and_i32(temp0, temp6, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_c_mnretEv
static void emit_qc_c_mnret(DisasContext *ctx, TCGv_env env) {
TCGv_i32 temp26 = xqci_csrr(ctx, env, 1993);
TCGv_i32 temp22 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp22, temp26, -1425076225);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp26, 28);
TCGv_i32 temp19 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp19, temp0, 1);
xqci_csrw_field(ctx, env, 768, 8, temp19);
xqci_csrw_field(ctx, env, 1860, 8, tcg_constant_i32(1));
tcg_gen_ori_i32(temp0, temp22, 268435456);
tcg_gen_shli_i32(temp22, temp19, 26);
tcg_gen_or_i32(temp0, temp22, temp0);
tcg_gen_shri_i32(temp19, temp26, 8);
tcg_gen_andi_i32(temp22, temp19, 61440);
tcg_gen_or_i32(temp22, temp0, temp22);
tcg_gen_ori_i32(temp0, temp22, 1089470464);
xqci_csrw(ctx, env, 1993, temp0);
temp22 = xqci_csrr_field(ctx, env, 1860, 6144);
TCGLabel * label32 = gen_new_label();
TCGLabel * label33 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp22, 3, label32);
gen_set_label(label33);
xqci_csrw_field(ctx, env, 768, 131072, tcg_constant_i32(0));
temp19 = xqci_implemented_Smdbltrp(ctx);
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp19, 0, label34);
tcg_gen_br(label32);
gen_set_label(label34);
xqci_csrw_field(ctx, env, 784, 1024, tcg_constant_i32(0));
tcg_gen_br(label32);
gen_set_label(label32);
TCGv_i32 temp16 = xqci_csrr_field(ctx, env, 1860, 6144);
TCGLabel * label36 = gen_new_label();
TCGLabel * label37 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp16, 0, label36);
gen_set_label(label37);
TCGv_i32 temp14 = xqci_csrr_field(ctx, env, 1860, 6144);
TCGLabel * label38 = gen_new_label();
TCGLabel * label39 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp14, 1, label38);
gen_set_label(label39);
TCGv_i32 temp12 = xqci_csrr_field(ctx, env, 1860, 6144);
TCGLabel * label40 = gen_new_label();
TCGLabel * label41 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp12, 3, label40);
gen_set_label(label41);
xqci_set_mode_M(ctx);
tcg_gen_br(label40);
gen_set_label(label38);
xqci_set_mode_S(ctx);
tcg_gen_br(label40);
gen_set_label(label36);
xqci_set_mode_U(ctx);
tcg_gen_br(label40);
gen_set_label(label40);
TCGv_i32 temp7 = xqci_implemented_U(ctx);
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp7, tcg_constant_i32(0), tcg_constant_i32(0), tcg_constant_i32(3));
xqci_csrw_field(ctx, env, 1860, 6144, temp0);
temp0 = xqci_csrr(ctx, env, 1857);
tcg_gen_mov_i32(cpu_pc, temp0);
}

// void _ZN12CPUArchState9qc_c_mretEv
static void emit_qc_c_mret(DisasContext *ctx, TCGv_env env) {
TCGv_i32 temp24 = xqci_csrr(ctx, env, 1993);
TCGv_i32 temp16 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp16, temp24, -739241985);
TCGv_i32 temp18 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp18, temp24, 27);
TCGv_i32 temp14 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp14, temp18, 1);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp24, 29);
tcg_gen_extract_i32(temp18, temp0, 0, 8);
tcg_gen_andi_i32(temp0, temp18, 1);
xqci_csrw_field(ctx, env, 768, 8, temp14);
xqci_csrw_field(ctx, env, 768, 128, tcg_constant_i32(1));
TCGv_i32 temp31 = xqci_implemented_Smdbltrp(ctx);
TCGLabel * label40 = gen_new_label();
TCGLabel * label41 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp31, 0, label40);
tcg_gen_br(label41);
gen_set_label(label40);
xqci_csrw_field(ctx, env, 784, 1024, temp0);
tcg_gen_br(label41);
gen_set_label(label41);
tcg_gen_ori_i32(temp18, temp16, 134217728);
tcg_gen_shli_i32(temp16, temp14, 26);
tcg_gen_or_i32(temp18, temp16, temp18);
tcg_gen_shri_i32(temp14, temp24, 4);
tcg_gen_andi_i32(temp16, temp14, 61440);
tcg_gen_or_i32(temp16, temp18, temp16);
tcg_gen_ori_i32(temp18, temp16, 983040);
xqci_csrw(ctx, env, 1993, temp18);
TCGLabel * label43 = gen_new_label();
TCGLabel * label44 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp0, 0, label43);
gen_set_label(label44);
temp18 = xqci_csrr_field(ctx, env, 768, 6144);
TCGLabel * label45 = gen_new_label();
TCGLabel * label46 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp18, 3, label45);
gen_set_label(label46);
xqci_csrw_field(ctx, env, 768, 131072, tcg_constant_i32(0));
tcg_gen_br(label45);
gen_set_label(label45);
temp16 = xqci_csrr_field(ctx, env, 768, 6144);
TCGLabel * label47 = gen_new_label();
TCGLabel * label48 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp16, 0, label47);
gen_set_label(label48);
temp14 = xqci_csrr_field(ctx, env, 768, 6144);
TCGLabel * label49 = gen_new_label();
TCGLabel * label50 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp14, 1, label49);
gen_set_label(label50);
TCGv_i32 temp12 = xqci_csrr_field(ctx, env, 768, 6144);
TCGLabel * label51 = gen_new_label();
TCGLabel * label52 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp12, 3, label51);
gen_set_label(label52);
xqci_set_mode_M(ctx);
tcg_gen_br(label51);
gen_set_label(label49);
xqci_set_mode_S(ctx);
tcg_gen_br(label51);
gen_set_label(label47);
xqci_set_mode_U(ctx);
tcg_gen_br(label51);
gen_set_label(label51);
TCGv_i32 temp7 = xqci_implemented_U(ctx);
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp7, tcg_constant_i32(0), tcg_constant_i32(0), tcg_constant_i32(3));
xqci_csrw_field(ctx, env, 768, 6144, temp0);
tcg_gen_br(label43);
gen_set_label(label43);
temp0 = xqci_csrr(ctx, env, 833);
tcg_gen_mov_i32(cpu_pc, temp0);
}

// void _ZN12CPUArchState12qc_c_muliaddE4BitsILj5ELb0EEhh
static void emit_qc_c_muliadd(DisasContext *ctx, TCGv_env env, uint8_t vi_8, uint8_t vi_14, uint8_t vi_17) {
TCGv_i32 temp5 = xqci_get_gpr(ctx, ((vi_17 & 7) | 8));
TCGv_i32 temp6 = xqci_get_gpr(ctx, ((vi_14 & 7) | 8));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_muli_i32(temp0, temp6, (vi_8 & 31));
tcg_gen_add_i32(temp0, temp0, temp5);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) ((vi_17 & 7) | 8))], temp0);
}

// void _ZN12CPUArchState10qc_c_mveqzEhh
static void emit_qc_c_mveqz(DisasContext *ctx, TCGv_env env, uint8_t vi_8, uint8_t vi_16) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp13 = xqci_get_gpr(ctx, ((vi_16 & 7) | 8));
tcg_gen_mov_i32(temp0, temp13);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp13, 0, label22);
gen_set_label(label23);
TCGv_i32 temp5 = xqci_get_gpr(ctx, ((vi_8 & 7) | 8));
tcg_gen_mov_i32(temp0, temp5);
tcg_gen_br(label22);
gen_set_label(label22);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) ((vi_16 & 7) | 8))], temp0);
}

// void _ZN12CPUArchState11qc_c_ptraceEv
static void emit_qc_c_ptrace(DisasContext *ctx, TCGv_env env) {
xqci_syscall(ctx, 9, tcg_constant_i32(0));
}

// void _ZN12CPUArchState11qc_c_setintE4BitsILj5ELb0EE
static void emit_qc_c_setint(DisasContext *ctx, TCGv_env env, uint8_t vi_6) {
TCGv_i32 temp3 = xqci_csrr(ctx, env, 2032);
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp2, temp3, (1 << (vi_6 & 31)));
xqci_csrw(ctx, env, 2032, temp2);
}

// void _ZN12CPUArchState6qc_cloEhh
static void emit_qc_clo(DisasContext *ctx, TCGv_env env, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp6, temp8, -1);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_clzi_i32(temp0, temp6, 32);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_clrintiE4BitsILj10ELb0EE
static void emit_qc_clrinti(DisasContext *ctx, TCGv_env env, uint16_t vi_8) {
TCGv_i32 temp3 = xqci_csrr(ctx, env, ((((uint16_t) vi_8 >> 5) & 31) + 2032));
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp2, temp3, ((1 << (vi_8 & 31)) ^ -1));
xqci_csrw(ctx, env, ((((uint16_t) vi_8 >> 5) & 31) + 2032), temp2);
}

// void _ZN12CPUArchState12qc_compress2Ehh
static void emit_qc_compress2(DisasContext *ctx, TCGv_env env, uint8_t vi_31, uint8_t vi_4) {
TCGv_i32 temp15 = xqci_get_gpr(ctx, vi_31);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp15, 12);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp0, 1);
TCGv_i32 temp20 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp20, temp15, 8);
tcg_gen_andi_i32(temp0, temp20, 1);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp12, temp15, 2);
tcg_gen_andi_i32(temp20, temp12, 1);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp15, 1);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp10, temp15, 13);
tcg_gen_andi_i32(temp12, temp10, 2);
tcg_gen_or_i32(temp8, temp12, temp8);
tcg_gen_shli_i32(temp12, temp8, 2);
tcg_gen_shri_i32(temp10, temp15, 9);
tcg_gen_andi_i32(temp8, temp10, 2);
tcg_gen_or_i32(temp8, temp12, temp8);
tcg_gen_or_i32(temp0, temp8, temp0);
tcg_gen_shli_i32(temp8, temp0, 3);
tcg_gen_shri_i32(temp12, temp15, 4);
tcg_gen_andi_i32(temp0, temp12, 4);
tcg_gen_or_i32(temp0, temp8, temp0);
tcg_gen_shri_i32(temp12, temp15, 3);
tcg_gen_andi_i32(temp8, temp12, 2);
tcg_gen_or_i32(temp8, temp0, temp8);
tcg_gen_or_i32(temp20, temp8, temp20);
tcg_gen_shli_i32(temp8, temp20, 1);
tcg_gen_shri_i32(temp0, temp15, 28);
tcg_gen_andi_i32(temp20, temp0, 1);
tcg_gen_shri_i32(temp12, temp15, 24);
tcg_gen_andi_i32(temp0, temp12, 1);
tcg_gen_shri_i32(temp10, temp15, 18);
tcg_gen_andi_i32(temp12, temp10, 1);
TCGv_i32 temp13 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp13, temp15, 16);
tcg_gen_andi_i32(temp10, temp13, 1);
tcg_gen_shri_i32(temp13, temp15, 29);
tcg_gen_andi_i32(temp13, temp13, 2);
tcg_gen_or_i32(temp20, temp13, temp20);
tcg_gen_shli_i32(temp13, temp20, 2);
tcg_gen_shri_i32(temp20, temp15, 25);
tcg_gen_andi_i32(temp20, temp20, 2);
tcg_gen_or_i32(temp13, temp13, temp20);
tcg_gen_or_i32(temp0, temp13, temp0);
tcg_gen_shli_i32(temp13, temp0, 3);
tcg_gen_shri_i32(temp0, temp15, 20);
tcg_gen_andi_i32(temp0, temp0, 4);
tcg_gen_or_i32(temp0, temp13, temp0);
tcg_gen_shri_i32(temp13, temp15, 19);
tcg_gen_andi_i32(temp13, temp13, 2);
tcg_gen_or_i32(temp0, temp0, temp13);
tcg_gen_or_i32(temp0, temp0, temp12);
tcg_gen_shli_i32(temp0, temp0, 1);
tcg_gen_andi_i32(temp0, temp0, 254);
tcg_gen_or_i32(temp0, temp0, temp10);
tcg_gen_andi_i32(temp8, temp8, 254);
tcg_gen_or_i32(temp6, temp6, temp8);
tcg_gen_shli_i32(temp0, temp0, 8);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState12qc_compress3Ehh
static void emit_qc_compress3(DisasContext *ctx, TCGv_env env, uint8_t vi_27, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_27);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp6, temp13, 18);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp6, 1);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp10, temp13, 12);
tcg_gen_andi_i32(temp6, temp10, 1);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp11, temp13, 3);
tcg_gen_andi_i32(temp10, temp11, 1);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp13, 1);
tcg_gen_shri_i32(temp11, temp13, 20);
tcg_gen_andi_i32(temp11, temp11, 2);
tcg_gen_or_i32(temp8, temp11, temp8);
tcg_gen_shli_i32(temp11, temp8, 2);
tcg_gen_shri_i32(temp8, temp13, 14);
tcg_gen_andi_i32(temp8, temp8, 2);
tcg_gen_or_i32(temp8, temp11, temp8);
tcg_gen_or_i32(temp6, temp8, temp6);
tcg_gen_shli_i32(temp8, temp6, 3);
tcg_gen_shri_i32(temp11, temp13, 7);
tcg_gen_andi_i32(temp6, temp11, 4);
tcg_gen_or_i32(temp6, temp8, temp6);
tcg_gen_shri_i32(temp11, temp13, 5);
tcg_gen_andi_i32(temp8, temp11, 2);
tcg_gen_or_i32(temp8, temp6, temp8);
tcg_gen_or_i32(temp10, temp8, temp10);
tcg_gen_shli_i32(temp8, temp10, 1);
tcg_gen_shri_i32(temp6, temp13, 24);
tcg_gen_andi_i32(temp10, temp6, 1);
tcg_gen_shri_i32(temp11, temp13, 28);
tcg_gen_andi_i32(temp6, temp11, 4);
tcg_gen_shri_i32(temp11, temp13, 26);
tcg_gen_andi_i32(temp11, temp11, 2);
tcg_gen_or_i32(temp6, temp6, temp11);
tcg_gen_or_i32(temp6, temp6, temp10);
tcg_gen_andi_i32(temp8, temp8, 254);
tcg_gen_or_i32(temp0, temp0, temp8);
tcg_gen_shli_i32(temp6, temp6, 8);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState6qc_ctoEhh
static void emit_qc_cto(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_12);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp0, temp9, -1);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_ctzi_i32(temp6, temp0, 32);
tcg_gen_addi_i32(temp0, temp6, 1);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_e_addaiE4BitsILj32ELb0EEh
static void emit_qc_e_addai(DisasContext *ctx, TCGv_env env, uint32_t vi_7, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp6, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_e_addiE4BitsILj26ELb0EEhh
static void emit_qc_e_addi(DisasContext *ctx, TCGv_env env, uint32_t vi_8, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp6, ((int32_t) (vi_8 << 6) >> 6));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_e_andaiE4BitsILj32ELb0EEh
static void emit_qc_e_andai(DisasContext *ctx, TCGv_env env, uint32_t vi_7, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp6, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_e_andiE4BitsILj26ELb0EEhh
static void emit_qc_e_andi(DisasContext *ctx, TCGv_env env, uint32_t vi_8, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp6, ((int32_t) (vi_8 << 6) >> 6));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_e_beqiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_beqi(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_9, uint8_t vi_12) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_12);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp8, ((int32_t) (int16_t) vi_9), label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState9qc_e_bgeiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_bgei(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_9, uint8_t vi_12) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_12);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LT, temp8, ((int32_t) (int16_t) vi_9), label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState10qc_e_bgeuiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_bgeui(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_10, uint8_t vi_13) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_13);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LTU, temp8, vi_10, label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState9qc_e_bltiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_blti(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_9, uint8_t vi_12) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_12);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GE, temp8, ((int32_t) (int16_t) vi_9), label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState10qc_e_bltuiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_bltui(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_10, uint8_t vi_13) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_13);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GEU, temp8, vi_10, label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState9qc_e_bneiE4BitsILj13ELb0EES0_ILj16ELb0EEh
static void emit_qc_e_bnei(DisasContext *ctx, TCGv_env env, uint16_t vi_3, uint16_t vi_9, uint8_t vi_12) {
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_12);
TCGLabel * label16 = gen_new_label();
TCGLabel * label17 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp8, ((int32_t) (int16_t) vi_9), label16);
gen_set_label(label17);
xqci_jump_conditional_pcrel(ctx, ((int32_t) ((vi_3 & 8191) << 19) >> 19));
tcg_gen_br(label16);
gen_set_label(label16);
xqci_jump_fallthrough_pcrel(ctx);
}

// void _ZN12CPUArchState6qc_e_jE4BitsILj32ELb0EE
static void emit_qc_e_j(DisasContext *ctx, TCGv_env env, uint32_t vi_0) {
xqci_jump_pcrel(ctx, vi_0);
}

// void _ZN12CPUArchState8qc_e_jalE4BitsILj32ELb0EE
static void emit_qc_e_jal(DisasContext *ctx, TCGv_env env, uint32_t vi_0) {
uint64_t vi_7 = xqci_current_pc(ctx);
tcg_gen_movi_i32(cpu_gpr[1ull], (vi_7 + 6));
xqci_jump_pcrel(ctx, vi_0);
}

// void _ZN12CPUArchState7qc_e_lbE4BitsILj26ELb0EEhh
static void emit_qc_e_lb(DisasContext *ctx, TCGv_env env, uint32_t vi_12, uint8_t vi_15, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_15);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp10, ((int32_t) (vi_12 << 6) >> 6));
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_qemu_ld_i32(temp7, temp0, ctx->mem_idx, MO_UB);
tcg_gen_shli_i32(temp0, temp7, 24);
tcg_gen_sari_i32(temp0, temp0, 24);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_e_lbuE4BitsILj26ELb0EEhh
static void emit_qc_e_lbu(DisasContext *ctx, TCGv_env env, uint32_t vi_11, uint8_t vi_14, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp6, temp9, ((int32_t) (vi_11 << 6) >> 6));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_UB);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_e_lhE4BitsILj26ELb0EEhh
static void emit_qc_e_lh(DisasContext *ctx, TCGv_env env, uint32_t vi_13, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp11, ((int32_t) (vi_13 << 6) >> 6));
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_qemu_ld_i32(temp7, temp0, ctx->mem_idx, MO_LEUW);
tcg_gen_shli_i32(temp0, temp7, 16);
tcg_gen_sari_i32(temp0, temp0, 16);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_e_lhuE4BitsILj26ELb0EEhh
static void emit_qc_e_lhu(DisasContext *ctx, TCGv_env env, uint32_t vi_12, uint8_t vi_15, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_15);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp6, temp10, ((int32_t) (vi_12 << 6) >> 6));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_LEUW);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_e_liE4BitsILj32ELb0EEh
static void emit_qc_e_li(DisasContext *ctx, TCGv_env env, uint32_t vi_0, uint8_t vi_5) {
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_5)], vi_0);
}

// void _ZN12CPUArchState7qc_e_lwE4BitsILj26ELb0EEhh
static void emit_qc_e_lw(DisasContext *ctx, TCGv_env env, uint32_t vi_11, uint8_t vi_14, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp6, temp9, ((int32_t) (vi_11 << 6) >> 6));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_LESL);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_e_oraiE4BitsILj32ELb0EEh
static void emit_qc_e_orai(DisasContext *ctx, TCGv_env env, uint32_t vi_7, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp0, temp6, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_e_oriE4BitsILj26ELb0EEhh
static void emit_qc_e_ori(DisasContext *ctx, TCGv_env env, uint32_t vi_8, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp0, temp6, ((int32_t) (vi_8 << 6) >> 6));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_e_sbE4BitsILj26ELb0EEhh
static void emit_qc_e_sb(DisasContext *ctx, TCGv_env env, uint32_t vi_11, uint8_t vi_7, uint8_t vi_14) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp9, ((int32_t) (vi_11 << 6) >> 6));
TCGv_i32 temp4 = xqci_get_gpr(ctx, vi_7);
TCGv_i32 temp1 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp1, temp4, 255);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_UB);
}

// void _ZN12CPUArchState7qc_e_shE4BitsILj26ELb0EEhh
static void emit_qc_e_sh(DisasContext *ctx, TCGv_env env, uint32_t vi_11, uint8_t vi_7, uint8_t vi_14) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp9, ((int32_t) (vi_11 << 6) >> 6));
TCGv_i32 temp4 = xqci_get_gpr(ctx, vi_7);
TCGv_i32 temp1 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp1, temp4, 65535);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_LEUW);
}

// void _ZN12CPUArchState7qc_e_swE4BitsILj26ELb0EEhh
static void emit_qc_e_sw(DisasContext *ctx, TCGv_env env, uint32_t vi_9, uint8_t vi_5, uint8_t vi_12) {
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_12);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp0, temp7, ((int32_t) (vi_9 << 6) >> 6));
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_5);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_LEUL);
}

// void _ZN12CPUArchState10qc_e_xoraiE4BitsILj32ELb0EEh
static void emit_qc_e_xorai(DisasContext *ctx, TCGv_env env, uint32_t vi_7, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp0, temp6, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_e_xoriE4BitsILj26ELb0EEhh
static void emit_qc_e_xori(DisasContext *ctx, TCGv_env env, uint32_t vi_8, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp0, temp6, ((int32_t) (vi_8 << 6) >> 6));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_expand2Ehh
static void emit_qc_expand2(DisasContext *ctx, TCGv_env env, uint8_t vi_45, uint8_t vi_4) {
TCGv_i32 temp24 = xqci_get_gpr(ctx, vi_45);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_extract_i32(temp10, temp24, 0, 8);
TCGv_i32 temp33 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp33, temp10, 3);
TCGv_i32 temp14 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp14, temp33, 1);
TCGv_i32 temp41 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp41, temp10, 2);
TCGv_i32 temp42 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp42, temp41, 1);
TCGv_i32 temp13 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp13, temp10, 1);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp13, 1);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp24, 1);
tcg_gen_shli_i32(temp13, temp33, 1);
tcg_gen_andi_i32(temp33, temp13, 2);
tcg_gen_or_i32(temp14, temp33, temp14);
tcg_gen_shli_i32(temp33, temp14, 2);
tcg_gen_shli_i32(temp14, temp41, 1);
tcg_gen_andi_i32(temp41, temp14, 2);
tcg_gen_or_i32(temp41, temp33, temp41);
tcg_gen_or_i32(temp42, temp41, temp42);
tcg_gen_shli_i32(temp41, temp42, 1);
tcg_gen_or_i32(temp42, temp41, temp8);
tcg_gen_shli_i32(temp33, temp42, 2);
tcg_gen_shli_i32(temp41, temp8, 1);
tcg_gen_or_i32(temp8, temp33, temp41);
tcg_gen_or_i32(temp33, temp8, temp6);
tcg_gen_shli_i32(temp8, temp33, 1);
tcg_gen_shri_i32(temp14, temp10, 6);
tcg_gen_andi_i32(temp13, temp14, 1);
tcg_gen_shri_i32(temp33, temp10, 5);
TCGv_i32 temp22 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp22, temp24, 4);
tcg_gen_andi_i32(temp10, temp22, 1);
TCGv_i32 temp19 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp19, temp24, 128);
tcg_gen_setcondi_i32(TCG_COND_EQ, temp22, temp19, 0);
tcg_gen_movcond_i32(TCG_COND_NE, temp19, temp22, tcg_constant_i32(0), tcg_constant_i32(0), tcg_constant_i32(12));
tcg_gen_shli_i32(temp22, temp14, 1);
tcg_gen_andi_i32(temp14, temp22, 2);
tcg_gen_or_i32(temp14, temp19, temp14);
tcg_gen_or_i32(temp13, temp14, temp13);
tcg_gen_shli_i32(temp14, temp13, 3);
tcg_gen_shli_i32(temp19, temp33, 2);
tcg_gen_andi_i32(temp13, temp19, 4);
tcg_gen_or_i32(temp13, temp14, temp13);
tcg_gen_shli_i32(temp14, temp33, 1);
tcg_gen_andi_i32(temp33, temp14, 2);
tcg_gen_or_i32(temp33, temp13, temp33);
tcg_gen_or_i32(temp14, temp10, temp33);
tcg_gen_shli_i32(temp13, temp14, 1);
tcg_gen_shri_i32(temp19, temp24, 11);
tcg_gen_extract_i32(temp14, temp19, 0, 8);
tcg_gen_andi_i32(temp19, temp14, 1);
tcg_gen_shri_i32(temp22, temp24, 10);
tcg_gen_extract_i32(temp14, temp22, 0, 8);
tcg_gen_andi_i32(temp22, temp14, 1);
TCGv_i32 temp15 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp15, temp24, 9);
tcg_gen_extract_i32(temp14, temp15, 0, 8);
tcg_gen_andi_i32(temp15, temp14, 1);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp24, 8);
tcg_gen_andi_i32(temp14, temp0, 1);
tcg_gen_muli_i32(temp0, temp19, 12);
TCGv_i32 temp29 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp29, temp22, 1);
tcg_gen_or_i32(temp29, temp0, temp29);
tcg_gen_or_i32(temp22, temp29, temp22);
tcg_gen_shli_i32(temp29, temp22, 3);
tcg_gen_shli_i32(temp0, temp15, 2);
tcg_gen_or_i32(temp0, temp29, temp0);
tcg_gen_shli_i32(temp29, temp15, 1);
tcg_gen_or_i32(temp29, temp0, temp29);
tcg_gen_or_i32(temp15, temp14, temp29);
tcg_gen_shli_i32(temp0, temp15, 1);
tcg_gen_shri_i32(temp22, temp24, 15);
tcg_gen_extract_i32(temp15, temp22, 0, 8);
tcg_gen_andi_i32(temp22, temp15, 1);
tcg_gen_shri_i32(temp19, temp24, 14);
tcg_gen_extract_i32(temp15, temp19, 0, 8);
tcg_gen_andi_i32(temp19, temp15, 1);
TCGv_i32 temp17 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp17, temp24, 13);
tcg_gen_extract_i32(temp15, temp17, 0, 8);
tcg_gen_andi_i32(temp17, temp15, 1);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp11, temp24, 12);
tcg_gen_andi_i32(temp15, temp11, 1);
tcg_gen_muli_i32(temp11, temp22, 12);
tcg_gen_shli_i32(temp22, temp19, 1);
tcg_gen_or_i32(temp11, temp11, temp22);
tcg_gen_or_i32(temp19, temp11, temp19);
tcg_gen_shli_i32(temp11, temp19, 3);
tcg_gen_shli_i32(temp19, temp17, 2);
tcg_gen_or_i32(temp11, temp11, temp19);
tcg_gen_shli_i32(temp17, temp17, 1);
tcg_gen_or_i32(temp17, temp11, temp17);
tcg_gen_or_i32(temp11, temp15, temp17);
tcg_gen_shli_i32(temp11, temp11, 1);
tcg_gen_andi_i32(temp11, temp11, 254);
tcg_gen_or_i32(temp11, temp11, temp15);
tcg_gen_andi_i32(temp0, temp0, 254);
tcg_gen_or_i32(temp0, temp0, temp14);
tcg_gen_andi_i32(temp13, temp13, 254);
tcg_gen_or_i32(temp10, temp10, temp13);
tcg_gen_shli_i32(temp11, temp11, 16);
tcg_gen_shli_i32(temp0, temp0, 8);
tcg_gen_or_i32(temp0, temp0, temp11);
tcg_gen_or_i32(temp0, temp0, temp10);
tcg_gen_andi_i32(temp8, temp8, 254);
tcg_gen_or_i32(temp6, temp6, temp8);
tcg_gen_shli_i32(temp0, temp0, 8);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_expand3Ehh
static void emit_qc_expand3(DisasContext *ctx, TCGv_env env, uint8_t vi_29, uint8_t vi_4) {
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_29);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_extract_i32(temp0, temp18, 0, 8);
TCGv_i32 temp16 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp16, temp0, 2);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp16, 1);
TCGv_i32 temp25 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp25, temp0, 1);
TCGv_i32 temp9 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp9, temp25, 1);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp0, 1);
tcg_gen_shli_i32(temp25, temp16, 1);
tcg_gen_andi_i32(temp16, temp25, 2);
tcg_gen_or_i32(temp25, temp16, temp8);
tcg_gen_shli_i32(temp16, temp25, 2);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp10, temp9, 1);
tcg_gen_or_i32(temp10, temp16, temp10);
tcg_gen_or_i32(temp16, temp10, temp9);
tcg_gen_shli_i32(temp10, temp16, 3);
tcg_gen_shli_i32(temp16, temp9, 2);
tcg_gen_or_i32(temp16, temp10, temp16);
tcg_gen_shli_i32(temp9, temp6, 1);
tcg_gen_or_i32(temp9, temp16, temp9);
tcg_gen_or_i32(temp16, temp9, temp6);
tcg_gen_shli_i32(temp9, temp16, 1);
tcg_gen_or_i32(temp6, temp9, temp6);
tcg_gen_shri_i32(temp16, temp0, 5);
tcg_gen_andi_i32(temp10, temp16, 1);
tcg_gen_shri_i32(temp25, temp0, 4);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp11, temp25, 1);
TCGv_i32 temp21 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp21, temp0, 3);
tcg_gen_andi_i32(temp9, temp21, 1);
tcg_gen_shli_i32(temp16, temp16, 1);
tcg_gen_andi_i32(temp16, temp16, 2);
tcg_gen_or_i32(temp16, temp16, temp11);
tcg_gen_shli_i32(temp16, temp16, 2);
tcg_gen_shli_i32(temp25, temp25, 1);
tcg_gen_andi_i32(temp25, temp25, 2);
tcg_gen_or_i32(temp16, temp16, temp25);
tcg_gen_or_i32(temp11, temp16, temp11);
tcg_gen_shli_i32(temp16, temp11, 3);
tcg_gen_shli_i32(temp11, temp9, 2);
tcg_gen_or_i32(temp11, temp16, temp11);
tcg_gen_shli_i32(temp16, temp21, 1);
tcg_gen_andi_i32(temp21, temp16, 2);
tcg_gen_or_i32(temp21, temp11, temp21);
tcg_gen_or_i32(temp9, temp21, temp9);
tcg_gen_shli_i32(temp21, temp9, 1);
tcg_gen_or_i32(temp8, temp21, temp8);
tcg_gen_shri_i32(temp9, temp0, 6);
tcg_gen_andi_i32(temp11, temp9, 1);
tcg_gen_andi_i32(temp16, temp9, 2);
tcg_gen_sextract_i32(temp0, temp0, 0, 8);
tcg_gen_movcond_i32(TCG_COND_LT, temp21, temp0, tcg_constant_i32(0), tcg_constant_i32(12), tcg_constant_i32(0));
tcg_gen_or_i32(temp0, temp21, temp16);
tcg_gen_or_i32(temp16, temp0, temp11);
tcg_gen_shli_i32(temp0, temp16, 3);
tcg_gen_shli_i32(temp16, temp11, 2);
tcg_gen_or_i32(temp16, temp0, temp16);
tcg_gen_shli_i32(temp0, temp9, 1);
tcg_gen_andi_i32(temp9, temp0, 2);
tcg_gen_or_i32(temp9, temp16, temp9);
tcg_gen_or_i32(temp16, temp9, temp10);
tcg_gen_shli_i32(temp9, temp16, 1);
tcg_gen_or_i32(temp10, temp9, temp10);
tcg_gen_shri_i32(temp16, temp18, 10);
tcg_gen_extract_i32(temp9, temp16, 0, 8);
tcg_gen_andi_i32(temp16, temp9, 1);
tcg_gen_shri_i32(temp0, temp18, 9);
tcg_gen_extract_i32(temp9, temp0, 0, 8);
tcg_gen_andi_i32(temp0, temp9, 1);
tcg_gen_shri_i32(temp11, temp18, 8);
tcg_gen_extract_i32(temp9, temp11, 0, 8);
tcg_gen_andi_i32(temp11, temp9, 1);
tcg_gen_muli_i32(temp9, temp16, 12);
tcg_gen_shli_i32(temp16, temp0, 1);
tcg_gen_or_i32(temp9, temp9, temp16);
tcg_gen_or_i32(temp9, temp9, temp0);
tcg_gen_shli_i32(temp9, temp9, 3);
tcg_gen_shli_i32(temp0, temp0, 2);
tcg_gen_or_i32(temp0, temp9, temp0);
tcg_gen_shli_i32(temp9, temp11, 1);
tcg_gen_or_i32(temp9, temp0, temp9);
tcg_gen_or_i32(temp0, temp9, temp11);
tcg_gen_shli_i32(temp9, temp0, 1);
tcg_gen_or_i32(temp11, temp9, temp11);
tcg_gen_shli_i32(temp0, temp11, 16);
tcg_gen_shli_i32(temp9, temp10, 8);
tcg_gen_or_i32(temp0, temp0, temp9);
tcg_gen_or_i32(temp0, temp0, temp8);
tcg_gen_shli_i32(temp0, temp0, 8);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState6qc_extE4BitsILj5ELb0EES1_hh
static void emit_qc_ext(DisasContext *ctx, TCGv_env env, uint8_t vi_21, uint8_t vi_15, uint8_t vi_18, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_18);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp7, temp13, (vi_15 & 31));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp6, (((vi_21 & 31) == 31) ? tcg_constant_i32(-1) : tcg_constant_i32(((-1 << ((vi_21 & 31) + 1)) ^ -1))));
tcg_gen_and_i32(temp6, temp7, temp6);
tcg_gen_shli_i32(temp7, temp6, (32 - ((vi_21 & 31) + 1)));
tcg_gen_sari_i32(temp7, temp7, (32 - ((vi_21 & 31) + 1)));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp0, (((vi_21 & 31) == 31) ? temp6 : temp7));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_extdE4BitsILj5ELb0EES1_hh
static void emit_qc_extd(DisasContext *ctx, TCGv_env env, uint8_t vi_18, uint8_t vi_15, uint8_t vi_25, uint8_t vi_4) {
TCGv_i32 temp23 = xqci_get_gpr(ctx, (vi_25 + 1));
TCGv_i64 temp11 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp11, temp23);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp3, temp11, 32ull);
TCGv_i32 temp20 = xqci_get_gpr(ctx, vi_25);
tcg_gen_extu_i32_i64(temp11, temp20);
tcg_gen_or_i64(temp11, temp3, temp11);
tcg_gen_shri_i64(temp3, temp11, ((uint64_t) (uint32_t) (vi_15 & 31)));
tcg_gen_mov_i64(temp11, (((vi_18 & 31) == 31) ? tcg_constant_i64(4294967295ull) : tcg_constant_i64(((uint64_t) (uint32_t) ((-1 << ((vi_18 & 31) + 1)) ^ -1)))));
tcg_gen_and_i64(temp3, temp3, temp11);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_extrl_i64_i32(temp6, temp3);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp7, temp6, (32 - ((vi_18 & 31) + 1)));
tcg_gen_sari_i32(temp7, temp7, (32 - ((vi_18 & 31) + 1)));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp0, (((vi_18 & 31) == 31) ? temp6 : temp7));
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_extdprEhhh
static void emit_qc_extdpr(DisasContext *ctx, TCGv_env env, uint8_t vi_22, uint8_t vi_29, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp11 = tcg_temp_new_i32();
TCGv_i32 temp27 = xqci_get_gpr(ctx, (vi_29 + 1));
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp27);
TCGv_i64 temp14 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp14, temp3, 32ull);
TCGv_i32 temp24 = xqci_get_gpr(ctx, vi_29);
tcg_gen_extu_i32_i64(temp3, temp24);
tcg_gen_or_i64(temp3, temp14, temp3);
TCGv_i32 temp15 = xqci_get_gpr(ctx, vi_22);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp6, temp15, 8);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp6, 63);
tcg_gen_movi_i32(temp11, 32);
TCGLabel * label33 = gen_new_label();
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp8, 32, label33);
gen_set_label(label34);
tcg_gen_mov_i32(temp11, temp8);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label35 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp8, 0, label35);
gen_set_label(label33);
tcg_gen_mov_i32(temp8, temp11);
tcg_gen_andi_i32(temp6, temp15, 63);
tcg_gen_extu_i32_i64(temp14, temp6);
tcg_gen_shr_i64(temp3, temp3, temp14);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp10, tcg_constant_i32(-1), temp8);
tcg_gen_xori_i32(temp6, temp10, -1);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp12, temp8, tcg_constant_i32(32), temp6, tcg_constant_i32(-1));
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_and_i32(temp11, temp12, temp6);
tcg_gen_sub_i32(temp10, tcg_constant_i32(32), temp8);
tcg_gen_shl_i32(temp6, temp11, temp10);
tcg_gen_sar_i32(temp10, temp6, temp10);
tcg_gen_movcond_i32(TCG_COND_LTU, temp6, temp8, tcg_constant_i32(32), temp10, temp11);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label35);
gen_set_label(label35);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_extdprhEhhh
static void emit_qc_extdprh(DisasContext *ctx, TCGv_env env, uint8_t vi_23, uint8_t vi_30, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp6 = tcg_temp_new_i32();
TCGv_i32 temp28 = xqci_get_gpr(ctx, (vi_30 + 1));
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp28);
TCGv_i64 temp14 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp14, temp3, 32ull);
TCGv_i32 temp25 = xqci_get_gpr(ctx, vi_30);
tcg_gen_extu_i32_i64(temp3, temp25);
tcg_gen_or_i64(temp3, temp14, temp3);
TCGv_i32 temp16 = xqci_get_gpr(ctx, vi_23);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp11, temp16, 24);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp11, 63);
tcg_gen_movi_i32(temp6, 32);
TCGLabel * label34 = gen_new_label();
TCGLabel * label35 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp8, 32, label34);
gen_set_label(label35);
tcg_gen_mov_i32(temp6, temp8);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label36 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp8, 0, label36);
gen_set_label(label34);
tcg_gen_mov_i32(temp8, temp6);
tcg_gen_shri_i32(temp11, temp16, 16);
tcg_gen_andi_i32(temp6, temp11, 63);
tcg_gen_extu_i32_i64(temp14, temp6);
tcg_gen_shr_i64(temp3, temp3, temp14);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp10, tcg_constant_i32(-1), temp8);
tcg_gen_xori_i32(temp6, temp10, -1);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp12, temp8, tcg_constant_i32(32), temp6, tcg_constant_i32(-1));
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_and_i32(temp11, temp12, temp6);
tcg_gen_sub_i32(temp10, tcg_constant_i32(32), temp8);
tcg_gen_shl_i32(temp6, temp11, temp10);
tcg_gen_sar_i32(temp10, temp6, temp10);
tcg_gen_movcond_i32(TCG_COND_LTU, temp6, temp8, tcg_constant_i32(32), temp10, temp11);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label36);
gen_set_label(label36);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_extdrEhhh
static void emit_qc_extdr(DisasContext *ctx, TCGv_env env, uint8_t vi_22, uint8_t vi_29, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp11 = tcg_temp_new_i32();
TCGv_i32 temp27 = xqci_get_gpr(ctx, (vi_29 + 1));
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp27);
TCGv_i64 temp14 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp14, temp3, 32ull);
TCGv_i32 temp24 = xqci_get_gpr(ctx, vi_29);
tcg_gen_extu_i32_i64(temp3, temp24);
tcg_gen_or_i64(temp3, temp14, temp3);
TCGv_i32 temp15 = xqci_get_gpr(ctx, vi_22);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp6, temp15, 16);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp6, 63);
tcg_gen_movi_i32(temp11, 32);
TCGLabel * label33 = gen_new_label();
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp8, 32, label33);
gen_set_label(label34);
tcg_gen_mov_i32(temp11, temp8);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label35 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp8, 0, label35);
gen_set_label(label33);
tcg_gen_mov_i32(temp8, temp11);
tcg_gen_andi_i32(temp6, temp15, 63);
tcg_gen_extu_i32_i64(temp14, temp6);
tcg_gen_shr_i64(temp3, temp3, temp14);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp10, tcg_constant_i32(-1), temp8);
tcg_gen_xori_i32(temp6, temp10, -1);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp12, temp8, tcg_constant_i32(32), temp6, tcg_constant_i32(-1));
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_and_i32(temp11, temp12, temp6);
tcg_gen_sub_i32(temp10, tcg_constant_i32(32), temp8);
tcg_gen_shl_i32(temp6, temp11, temp10);
tcg_gen_sar_i32(temp10, temp6, temp10);
tcg_gen_movcond_i32(TCG_COND_LTU, temp6, temp8, tcg_constant_i32(32), temp10, temp11);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label35);
gen_set_label(label35);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_extduE4BitsILj5ELb0EES1_hh
static void emit_qc_extdu(DisasContext *ctx, TCGv_env env, uint8_t vi_15, uint8_t vi_12, uint8_t vi_22, uint8_t vi_4) {
TCGv_i32 temp20 = xqci_get_gpr(ctx, (vi_22 + 1));
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp20);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp3, temp6, 32ull);
TCGv_i32 temp17 = xqci_get_gpr(ctx, vi_22);
tcg_gen_extu_i32_i64(temp6, temp17);
tcg_gen_or_i64(temp6, temp3, temp6);
tcg_gen_shri_i64(temp3, temp6, ((uint64_t) (uint32_t) (vi_12 & 31)));
tcg_gen_mov_i64(temp6, (((vi_15 & 31) == 31) ? tcg_constant_i64(4294967295ull) : tcg_constant_i64(((uint64_t) (uint32_t) ((-1 << ((vi_15 & 31) + 1)) ^ -1)))));
tcg_gen_and_i64(temp3, temp3, temp6);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_extrl_i64_i32(temp0, temp3);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_extduprEhhh
static void emit_qc_extdupr(DisasContext *ctx, TCGv_env env, uint8_t vi_22, uint8_t vi_29, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i64 temp8 = tcg_temp_new_i64();
TCGv_i32 temp27 = xqci_get_gpr(ctx, (vi_29 + 1));
TCGv_i64 temp7 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp7, temp27);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp3, temp7, 32ull);
TCGv_i32 temp24 = xqci_get_gpr(ctx, vi_29);
tcg_gen_extu_i32_i64(temp7, temp24);
tcg_gen_or_i64(temp7, temp3, temp7);
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_22);
TCGv_i32 temp14 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp14, temp9, 8);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp14, 63);
tcg_gen_movi_i64(temp8, 4294967295ull);
TCGLabel * label33 = gen_new_label();
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label33);
gen_set_label(label34);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label35 = gen_new_label();
TCGLabel * label36 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label35);
gen_set_label(label36);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp11, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp14, temp11, -1);
tcg_gen_movcond_i32(TCG_COND_LTU, temp11, temp6, tcg_constant_i32(32), temp14, tcg_constant_i32(-1));
tcg_gen_extu_i32_i64(temp3, temp11);
tcg_gen_mov_i64(temp8, temp3);
tcg_gen_br(label33);
gen_set_label(label33);
tcg_gen_mov_i64(temp3, temp8);
tcg_gen_andi_i32(temp6, temp9, 63);
tcg_gen_extu_i32_i64(temp8, temp6);
tcg_gen_shr_i64(temp7, temp7, temp8);
tcg_gen_and_i64(temp3, temp3, temp7);
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label35);
gen_set_label(label35);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState11qc_extduprhEhhh
static void emit_qc_extduprh(DisasContext *ctx, TCGv_env env, uint8_t vi_23, uint8_t vi_30, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i64 temp8 = tcg_temp_new_i64();
TCGv_i32 temp28 = xqci_get_gpr(ctx, (vi_30 + 1));
TCGv_i64 temp7 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp7, temp28);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp3, temp7, 32ull);
TCGv_i32 temp25 = xqci_get_gpr(ctx, vi_30);
tcg_gen_extu_i32_i64(temp7, temp25);
tcg_gen_or_i64(temp7, temp3, temp7);
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_23);
TCGv_i32 temp15 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp15, temp10, 24);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp15, 63);
tcg_gen_movi_i64(temp8, 4294967295ull);
TCGLabel * label34 = gen_new_label();
TCGLabel * label35 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label34);
gen_set_label(label35);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label36 = gen_new_label();
TCGLabel * label37 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label36);
gen_set_label(label37);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp12, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp15, temp12, -1);
tcg_gen_movcond_i32(TCG_COND_LTU, temp12, temp6, tcg_constant_i32(32), temp15, tcg_constant_i32(-1));
tcg_gen_extu_i32_i64(temp3, temp12);
tcg_gen_mov_i64(temp8, temp3);
tcg_gen_br(label34);
gen_set_label(label34);
tcg_gen_mov_i64(temp3, temp8);
tcg_gen_shri_i32(temp6, temp10, 16);
tcg_gen_andi_i32(temp6, temp6, 63);
tcg_gen_extu_i32_i64(temp8, temp6);
tcg_gen_shr_i64(temp7, temp7, temp8);
tcg_gen_and_i64(temp3, temp3, temp7);
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label36);
gen_set_label(label36);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_extdurEhhh
static void emit_qc_extdur(DisasContext *ctx, TCGv_env env, uint8_t vi_22, uint8_t vi_29, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i64 temp8 = tcg_temp_new_i64();
TCGv_i32 temp27 = xqci_get_gpr(ctx, (vi_29 + 1));
TCGv_i64 temp7 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp7, temp27);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp3, temp7, 32ull);
TCGv_i32 temp24 = xqci_get_gpr(ctx, vi_29);
tcg_gen_extu_i32_i64(temp7, temp24);
tcg_gen_or_i64(temp7, temp3, temp7);
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_22);
TCGv_i32 temp14 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp14, temp9, 16);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp14, 63);
tcg_gen_movi_i64(temp8, 4294967295ull);
TCGLabel * label33 = gen_new_label();
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label33);
gen_set_label(label34);
tcg_gen_movi_i32(temp0, 0);
TCGLabel * label35 = gen_new_label();
TCGLabel * label36 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label35);
gen_set_label(label36);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp11, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp14, temp11, -1);
tcg_gen_movcond_i32(TCG_COND_LTU, temp11, temp6, tcg_constant_i32(32), temp14, tcg_constant_i32(-1));
tcg_gen_extu_i32_i64(temp3, temp11);
tcg_gen_mov_i64(temp8, temp3);
tcg_gen_br(label33);
gen_set_label(label33);
tcg_gen_mov_i64(temp3, temp8);
tcg_gen_andi_i32(temp6, temp9, 63);
tcg_gen_extu_i32_i64(temp8, temp6);
tcg_gen_shr_i64(temp7, temp7, temp8);
tcg_gen_and_i64(temp3, temp3, temp7);
tcg_gen_extrl_i64_i32(temp6, temp3);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label35);
gen_set_label(label35);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_extuE4BitsILj5ELb0EES1_hh
static void emit_qc_extu(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_13, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp11, (vi_13 & 31));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp6, (((vi_19 & 31) == 31) ? tcg_constant_i32(-1) : tcg_constant_i32(((-1 << ((vi_19 & 31) + 1)) ^ -1))));
tcg_gen_and_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_insbE4BitsILj5ELb0EES1_hh
static void emit_qc_insb(DisasContext *ctx, TCGv_env env, uint8_t vi_21, uint8_t vi_16, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp6, (((vi_21 & 31) == 31) ? tcg_constant_i32(-1) : tcg_constant_i32(((-1 << ((vi_21 & 31) + 1)) ^ -1))));
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp7, temp6, (vi_16 & 31));
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp0, temp7, -1);
tcg_gen_and_i32(temp6, temp13, temp0);
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_11);
tcg_gen_shli_i32(temp0, temp8, (vi_16 & 31));
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_insbhE4BitsILj5ELb0EES1_hh
static void emit_qc_insbh(DisasContext *ctx, TCGv_env env, uint8_t vi_25, uint8_t vi_21, uint8_t vi_13, uint8_t vi_4) {
TCGLabel * label32 = gen_new_label();
TCGLabel * label33 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LEU, tcg_constant_i32((((vi_25 & 31) + 1) + (vi_21 & 31))), 32, label32);
gen_set_label(label33);
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_13);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp9, (32 - (vi_21 & 31)));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp8, (-1 << ((((vi_25 & 31) + 1) + (vi_21 & 31)) + -32)));
tcg_gen_andi_i32(temp0, temp0, ((-1 << ((((vi_25 & 31) + 1) + (vi_21 & 31)) + -32)) ^ -1));
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label32);
gen_set_label(label32);
}

// void _ZN12CPUArchState9qc_insbhrEhhh
static void emit_qc_insbhr(DisasContext *ctx, TCGv_env env, uint8_t vi_25, uint8_t vi_12, uint8_t vi_4) {
TCGv_i32 temp9 = tcg_temp_new_i32();
TCGv_i32 temp20 = xqci_get_gpr(ctx, vi_25);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp7, temp20, 16);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp8, temp7, 63);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp20, 31);
TCGLabel * label29 = gen_new_label();
TCGLabel * label30 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp8, 32, label29);
gen_set_label(label30);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_add_i32(temp6, temp8, temp0);
tcg_gen_setcondi_i32(TCG_COND_GTU, temp7, temp6, 32);
tcg_gen_setcondi_i32(TCG_COND_NE, temp8, temp8, 0);
tcg_gen_and_i32(temp7, temp8, temp7);
tcg_gen_mov_i32(temp9, temp6);
TCGLabel * label31 = gen_new_label();
TCGLabel * label32 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp7, 0, label31);
tcg_gen_br(label32);
gen_set_label(label29);
tcg_gen_ori_i32(temp6, temp0, 32);
tcg_gen_mov_i32(temp9, temp6);
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 32, label32);
gen_set_label(label31);
tcg_gen_mov_i32(temp6, temp9);
tcg_gen_addi_i32(temp9, temp6, -32);
tcg_gen_shl_i32(temp6, tcg_constant_i32(-1), temp9);
tcg_gen_xori_i32(temp7, temp6, -1);
temp8 = xqci_get_gpr(ctx, vi_4);
temp9 = xqci_get_gpr(ctx, vi_12);
tcg_gen_sub_i32(temp0, tcg_constant_i32(32), temp0);
tcg_gen_shr_i32(temp0, temp9, temp0);
tcg_gen_and_i32(temp6, temp6, temp8);
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label32);
gen_set_label(label32);
}

// void _ZN12CPUArchState8qc_insbiE4BitsILj5ELb0EES1_S1_h
static void emit_qc_insbi(DisasContext *ctx, TCGv_env env, uint8_t vi_21, uint8_t vi_17, uint8_t vi_13, uint8_t vi_4) {
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_mov_i32(temp6, (((vi_21 & 31) == 31) ? tcg_constant_i32(-1) : tcg_constant_i32(((-1 << ((vi_21 & 31) + 1)) ^ -1))));
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp7, temp6, (vi_17 & 31));
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp0, temp7, -1);
tcg_gen_and_i32(temp0, temp9, temp0);
tcg_gen_andi_i32(temp6, temp7, (((int32_t) ((vi_13 & 31) << 27) >> 27) << (vi_17 & 31)));
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_insbprEhhh
static void emit_qc_insbpr(DisasContext *ctx, TCGv_env env, uint8_t vi_26, uint8_t vi_10, uint8_t vi_4) {
TCGv_i32 temp7 = tcg_temp_new_i32();
TCGv_i32 temp15 = xqci_get_gpr(ctx, vi_26);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp15, 8);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp0, 63);
tcg_gen_movi_i32(temp7, -1);
TCGLabel * label30 = gen_new_label();
TCGLabel * label31 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label30);
gen_set_label(label31);
TCGLabel * label32 = gen_new_label();
TCGLabel * label33 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label32);
gen_set_label(label33);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp12, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp0, temp12, -1);
TCGv_i32 temp17 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp17, temp6, tcg_constant_i32(32), temp0, tcg_constant_i32(-1));
tcg_gen_mov_i32(temp7, temp17);
tcg_gen_br(label30);
gen_set_label(label30);
tcg_gen_mov_i32(temp6, temp7);
tcg_gen_andi_i32(temp0, temp15, 31);
tcg_gen_shl_i32(temp7, temp6, temp0);
temp12 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp8, temp7, -1);
tcg_gen_and_i32(temp6, temp12, temp8);
temp8 = xqci_get_gpr(ctx, vi_10);
tcg_gen_shl_i32(temp0, temp8, temp0);
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label32);
gen_set_label(label32);
}

// void _ZN12CPUArchState10qc_insbprhEhhh
static void emit_qc_insbprh(DisasContext *ctx, TCGv_env env, uint8_t vi_27, uint8_t vi_10, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp16 = xqci_get_gpr(ctx, vi_27);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp7, temp16, 24);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp7, 63);
tcg_gen_movi_i32(temp0, -1);
TCGLabel * label31 = gen_new_label();
TCGLabel * label32 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label31);
gen_set_label(label32);
TCGLabel * label33 = gen_new_label();
TCGLabel * label34 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label33);
gen_set_label(label34);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp12, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp7, temp12, -1);
TCGv_i32 temp18 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp18, temp6, tcg_constant_i32(32), temp7, tcg_constant_i32(-1));
tcg_gen_mov_i32(temp0, temp18);
tcg_gen_br(label31);
gen_set_label(label31);
tcg_gen_mov_i32(temp6, temp0);
tcg_gen_shri_i32(temp7, temp16, 16);
tcg_gen_andi_i32(temp0, temp7, 31);
tcg_gen_shl_i32(temp7, temp6, temp0);
temp12 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp8, temp7, -1);
tcg_gen_and_i32(temp6, temp12, temp8);
temp8 = xqci_get_gpr(ctx, vi_10);
tcg_gen_shl_i32(temp0, temp8, temp0);
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label33);
gen_set_label(label33);
}

// void _ZN12CPUArchState8qc_insbrEhhh
static void emit_qc_insbr(DisasContext *ctx, TCGv_env env, uint8_t vi_26, uint8_t vi_10, uint8_t vi_4) {
TCGv_i32 temp7 = tcg_temp_new_i32();
TCGv_i32 temp15 = xqci_get_gpr(ctx, vi_26);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp15, 16);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp0, 63);
tcg_gen_movi_i32(temp7, -1);
TCGLabel * label30 = gen_new_label();
TCGLabel * label31 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp6, 32, label30);
gen_set_label(label31);
TCGLabel * label32 = gen_new_label();
TCGLabel * label33 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp6, 0, label32);
gen_set_label(label33);
TCGv_i32 temp12 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp12, tcg_constant_i32(-1), temp6);
tcg_gen_xori_i32(temp0, temp12, -1);
TCGv_i32 temp17 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp17, temp6, tcg_constant_i32(32), temp0, tcg_constant_i32(-1));
tcg_gen_mov_i32(temp7, temp17);
tcg_gen_br(label30);
gen_set_label(label30);
tcg_gen_mov_i32(temp6, temp7);
tcg_gen_andi_i32(temp0, temp15, 31);
tcg_gen_shl_i32(temp7, temp6, temp0);
temp12 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp8, temp7, -1);
tcg_gen_and_i32(temp6, temp12, temp8);
temp8 = xqci_get_gpr(ctx, vi_10);
tcg_gen_shl_i32(temp0, temp8, temp0);
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label32);
gen_set_label(label32);
}

// void _ZN12CPUArchState9qc_insbriE4BitsILj11ELb0EEhh
static void emit_qc_insbri(DisasContext *ctx, TCGv_env env, uint16_t vi_13, uint8_t vi_28, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp17 = xqci_get_gpr(ctx, vi_28);
TCGv_i32 temp8 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp8, temp17, 16);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp7, temp8, 63);
tcg_gen_movi_i32(temp0, -1);
TCGLabel * label32 = gen_new_label();
TCGLabel * label33 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GTU, temp7, 32, label32);
gen_set_label(label33);
TCGLabel * label34 = gen_new_label();
TCGLabel * label35 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp7, 0, label34);
gen_set_label(label35);
TCGv_i32 temp9 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp9, tcg_constant_i32(-1), temp7);
tcg_gen_xori_i32(temp8, temp9, -1);
TCGv_i32 temp19 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LTU, temp19, temp7, tcg_constant_i32(32), temp8, tcg_constant_i32(-1));
tcg_gen_mov_i32(temp0, temp19);
tcg_gen_br(label32);
gen_set_label(label32);
tcg_gen_mov_i32(temp7, temp0);
tcg_gen_andi_i32(temp8, temp17, 31);
tcg_gen_shl_i32(temp0, temp7, temp8);
temp9 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp6, temp0, -1);
tcg_gen_and_i32(temp6, temp9, temp6);
tcg_gen_shl_i32(temp7, tcg_constant_i32(((int32_t) ((vi_13 & 2047) << 21) >> 21)), temp8);
tcg_gen_and_i32(temp0, temp0, temp7);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label34);
gen_set_label(label34);
}

// void _ZN12CPUArchState5qc_liE4BitsILj20ELb0EEh
static void emit_qc_li(DisasContext *ctx, TCGv_env env, uint32_t vi_7, uint8_t vi_4) {
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) (vi_7 << 12) >> 12));
}

// void _ZN12CPUArchState7qc_lieqEhh4BitsILj5ELb0EEh
static void emit_qc_lieq(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_NE, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState8qc_lieqiEh4BitsILj5ELb0EES1_h
static void emit_qc_lieqi(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGLabel * label26 = gen_new_label();
TCGLabel * label27 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp13, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_15 << 3)) >> 3)), label26);
gen_set_label(label27);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label26);
gen_set_label(label26);
}

// void _ZN12CPUArchState7qc_ligeEhh4BitsILj5ELb0EEh
static void emit_qc_lige(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_LT, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState8qc_ligeiEh4BitsILj5ELb0EES1_h
static void emit_qc_ligei(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGLabel * label26 = gen_new_label();
TCGLabel * label27 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LT, temp13, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_15 << 3)) >> 3)), label26);
gen_set_label(label27);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label26);
gen_set_label(label26);
}

// void _ZN12CPUArchState8qc_ligeuEhh4BitsILj5ELb0EEh
static void emit_qc_ligeu(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_LTU, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState9qc_ligeuiEh4BitsILj5ELb0EES1_h
static void emit_qc_ligeui(DisasContext *ctx, TCGv_env env, uint8_t vi_18, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_18);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LTU, temp13, (vi_15 & 31), label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState7qc_liltEhh4BitsILj5ELb0EEh
static void emit_qc_lilt(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_GE, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState8qc_liltiEh4BitsILj5ELb0EES1_h
static void emit_qc_lilti(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGLabel * label26 = gen_new_label();
TCGLabel * label27 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GE, temp13, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_15 << 3)) >> 3)), label26);
gen_set_label(label27);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label26);
gen_set_label(label26);
}

// void _ZN12CPUArchState8qc_liltuEhh4BitsILj5ELb0EEh
static void emit_qc_liltu(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_GEU, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState9qc_liltuiEh4BitsILj5ELb0EES1_h
static void emit_qc_liltui(DisasContext *ctx, TCGv_env env, uint8_t vi_18, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_18);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GEU, temp13, (vi_15 & 31), label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState7qc_lineEhh4BitsILj5ELb0EEh
static void emit_qc_line(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_16, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGLabel * label22 = gen_new_label();
TCGLabel * label23 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp13, temp14, label22);
gen_set_label(label23);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label22);
gen_set_label(label22);
}

// void _ZN12CPUArchState8qc_lineiEh4BitsILj5ELb0EES1_h
static void emit_qc_linei(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_15, uint8_t vi_8, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_19);
TCGLabel * label26 = gen_new_label();
TCGLabel * label27 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp13, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_15 << 3)) >> 3)), label26);
gen_set_label(label27);
tcg_gen_movi_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], ((int32_t) ((vi_8 & 31) << 27) >> 27));
tcg_gen_br(label26);
gen_set_label(label26);
}

// void _ZN12CPUArchState6qc_lrbE4BitsILj3ELb0EEhhh
static void emit_qc_lrb(DisasContext *ctx, TCGv_env env, uint8_t vi_13, uint8_t vi_20, uint8_t vi_17, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_20);
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp7, temp11, (vi_13 & 7));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp7, temp10);
tcg_gen_qemu_ld_i32(temp7, temp0, ctx->mem_idx, MO_UB);
tcg_gen_shli_i32(temp0, temp7, 24);
tcg_gen_sari_i32(temp0, temp0, 24);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_lrbuE4BitsILj3ELb0EEhhh
static void emit_qc_lrbu(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_19, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp0, temp10, (vi_12 & 7));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_add_i32(temp6, temp0, temp9);
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_UB);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState6qc_lrhE4BitsILj3ELb0EEhhh
static void emit_qc_lrh(DisasContext *ctx, TCGv_env env, uint8_t vi_14, uint8_t vi_21, uint8_t vi_18, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_21);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_18);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp7, temp12, (vi_14 & 7));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp7, temp11);
tcg_gen_qemu_ld_i32(temp7, temp0, ctx->mem_idx, MO_LEUW);
tcg_gen_shli_i32(temp0, temp7, 16);
tcg_gen_sari_i32(temp0, temp0, 16);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_lrhuE4BitsILj3ELb0EEhhh
static void emit_qc_lrhu(DisasContext *ctx, TCGv_env env, uint8_t vi_13, uint8_t vi_20, uint8_t vi_17, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_20);
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp0, temp11, (vi_13 & 7));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_add_i32(temp6, temp0, temp10);
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_LEUW);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState6qc_lrwE4BitsILj3ELb0EEhhh
static void emit_qc_lrw(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_19, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp0, temp10, (vi_12 & 7));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_add_i32(temp6, temp0, temp9);
tcg_gen_qemu_ld_i32(temp0, temp6, ctx->mem_idx, MO_LESL);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_muliaddE4BitsILj12ELb0EEhh
static void emit_qc_muliadd(DisasContext *ctx, TCGv_env env, uint16_t vi_10, uint8_t vi_14, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_muli_i32(temp0, temp7, ((int32_t) ((vi_10 & 4095) << 20) >> 20));
tcg_gen_add_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_mveqEhhhh
static void emit_qc_mveq(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_NE, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState8qc_mveqiEh4BitsILj5ELb0EEhh
static void emit_qc_mveqi(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label24 = gen_new_label();
TCGLabel * label25 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_NE, temp11, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_13 << 3)) >> 3)), label24);
gen_set_label(label25);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label24);
gen_set_label(label24);
}

// void _ZN12CPUArchState7qc_mvgeEhhhh
static void emit_qc_mvge(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_LT, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState8qc_mvgeiEh4BitsILj5ELb0EEhh
static void emit_qc_mvgei(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label24 = gen_new_label();
TCGLabel * label25 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LT, temp11, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_13 << 3)) >> 3)), label24);
gen_set_label(label25);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label24);
gen_set_label(label24);
}

// void _ZN12CPUArchState8qc_mvgeuEhhhh
static void emit_qc_mvgeu(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_LTU, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState9qc_mvgeuiEh4BitsILj5ELb0EEhh
static void emit_qc_mvgeui(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_LTU, temp11, (vi_13 & 31), label21);
gen_set_label(label22);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label21);
gen_set_label(label21);
}

// void _ZN12CPUArchState7qc_mvltEhhhh
static void emit_qc_mvlt(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_GE, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState8qc_mvltiEh4BitsILj5ELb0EEhh
static void emit_qc_mvlti(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label24 = gen_new_label();
TCGLabel * label25 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GE, temp11, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_13 << 3)) >> 3)), label24);
gen_set_label(label25);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label24);
gen_set_label(label24);
}

// void _ZN12CPUArchState8qc_mvltuEhhhh
static void emit_qc_mvltu(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_GEU, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState9qc_mvltuiEh4BitsILj5ELb0EEhh
static void emit_qc_mvltui(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label21 = gen_new_label();
TCGLabel * label22 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_GEU, temp11, (vi_13 & 31), label21);
gen_set_label(label22);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label21);
gen_set_label(label21);
}

// void _ZN12CPUArchState7qc_mvneEhhhh
static void emit_qc_mvne(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_14, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
TCGLabel * label20 = gen_new_label();
TCGLabel * label21 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp11, temp12, label20);
gen_set_label(label21);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label20);
gen_set_label(label20);
}

// void _ZN12CPUArchState8qc_mvneiEh4BitsILj5ELb0EEhh
static void emit_qc_mvnei(DisasContext *ctx, TCGv_env env, uint8_t vi_17, uint8_t vi_13, uint8_t vi_7, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_17);
TCGLabel * label24 = gen_new_label();
TCGLabel * label25 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp11, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_13 << 3)) >> 3)), label24);
gen_set_label(label25);
TCGv_i32 temp0 = xqci_get_gpr(ctx, vi_7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
tcg_gen_br(label24);
gen_set_label(label24);
}

// void _ZN12CPUArchState7qc_normEhh
static void emit_qc_norm(DisasContext *ctx, TCGv_env env, uint8_t vi_23, uint8_t vi_4) {
TCGv_i32 temp16 = xqci_get_gpr(ctx, vi_23);
TCGv_i32 temp20 = tcg_temp_new_i32();
tcg_gen_clzi_i32(temp20, temp16, 32);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_xori_i32(temp11, temp16, -1);
TCGv_i32 temp19 = tcg_temp_new_i32();
tcg_gen_clzi_i32(temp19, temp11, 32);
TCGv_i32 temp15 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp15, temp16, tcg_constant_i32(0), temp19, temp20);
TCGv_i32 temp9 = tcg_temp_new_i32();
tcg_gen_addi_i32(temp9, temp15, -1);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp6, temp16, temp9);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_sub_i32(temp0, tcg_constant_i32(1), temp15);
tcg_gen_andi_i32(temp11, temp6, -256);
tcg_gen_movcond_i32(TCG_COND_LTU, temp6, temp9, tcg_constant_i32(32), temp11, tcg_constant_i32(0));
tcg_gen_andi_i32(temp0, temp0, 255);
tcg_gen_or_i32(temp0, temp6, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_normeuEhh
static void emit_qc_normeu(DisasContext *ctx, TCGv_env env, uint8_t vi_15, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_15);
TCGv_i32 temp11 = tcg_temp_new_i32();
tcg_gen_clzi_i32(temp11, temp10, 32);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp6, temp11, 30);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp0, temp10, temp6);
tcg_gen_andi_i32(temp0, temp0, -256);
tcg_gen_sub_i32(temp6, tcg_constant_i32(0), temp6);
tcg_gen_andi_i32(temp6, temp6, 254);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_normuEhh
static void emit_qc_normu(DisasContext *ctx, TCGv_env env, uint8_t vi_14, uint8_t vi_4) {
TCGv_i32 temp11 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp9 = tcg_temp_new_i32();
tcg_gen_clzi_i32(temp9, temp11, 32);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_shl_i32(temp6, temp11, temp9);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp6, -256);
tcg_gen_sub_i32(temp6, tcg_constant_i32(0), temp9);
tcg_gen_andi_i32(temp6, temp6, 255);
tcg_gen_or_i32(temp0, temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState12qc_pcoredumpEv
static void emit_qc_pcoredump(DisasContext *ctx, TCGv_env env) {
xqci_syscall(ctx, 8, tcg_constant_i32(0));
}

// void _ZN12CPUArchState8qc_pexitEh
static void emit_qc_pexit(DisasContext *ctx, TCGv_env env, uint8_t vi_3) {
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_3);
xqci_syscall(ctx, 12, temp1);
}

// void _ZN12CPUArchState8qc_ppregEh
static void emit_qc_ppreg(DisasContext *ctx, TCGv_env env, uint8_t vi_3) {
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_3);
xqci_syscall(ctx, 2, temp1);
}

// void _ZN12CPUArchState9qc_ppregsEv
static void emit_qc_ppregs(DisasContext *ctx, TCGv_env env) {
xqci_syscall(ctx, 3, tcg_constant_i32(0));
}

// void _ZN12CPUArchState8qc_pputcEh
static void emit_qc_pputc(DisasContext *ctx, TCGv_env env, uint8_t vi_3) {
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_3);
xqci_syscall(ctx, 4, temp1);
}

// void _ZN12CPUArchState9qc_pputciE4BitsILj8ELb0EE
static void emit_qc_pputci(DisasContext *ctx, TCGv_env env, uint8_t vi_2) {
xqci_syscall(ctx, 5, tcg_constant_i32(vi_2));
}

// void _ZN12CPUArchState8qc_pputsEh
static void emit_qc_pputs(DisasContext *ctx, TCGv_env env, uint8_t vi_3) {
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_3);
xqci_syscall(ctx, 6, temp1);
}

// void _ZN12CPUArchState11qc_psyscallEh
static void emit_qc_psyscall(DisasContext *ctx, TCGv_env env, uint8_t vi_3) {
TCGv_i32 temp1 = xqci_get_gpr(ctx, vi_3);
xqci_syscall(ctx, 10, temp1);
}

// void _ZN12CPUArchState12qc_psyscalliE4BitsILj10ELb0EE
static void emit_qc_psyscalli(DisasContext *ctx, TCGv_env env, uint16_t vi_2) {
xqci_syscall(ctx, 11, tcg_constant_i32((vi_2 & 1023)));
}

// void _ZN12CPUArchState12qc_selecteqiE4BitsILj5ELb0EEhhh
static void emit_qc_selecteqi(DisasContext *ctx, TCGv_env env, uint8_t vi_16, uint8_t vi_12, uint8_t vi_10, uint8_t vi_4) {
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_setcondi_i32(TCG_COND_EQ, temp0, temp14, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_16 << 3)) >> 3)));
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_12);
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_10);
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp0, tcg_constant_i32(0), temp6, temp7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState12qc_selectieqEhh4BitsILj5ELb0EEh
static void emit_qc_selectieq(DisasContext *ctx, TCGv_env env, uint8_t vi_20, uint8_t vi_8, uint8_t vi_12, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp17 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_20);
TCGLabel * label25 = gen_new_label();
TCGLabel * label26 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp17, temp18, label25);
gen_set_label(label26);
tcg_gen_movi_i32(temp0, ((int32_t) ((vi_12 & 31) << 27) >> 27));
TCGLabel * label30 = gen_new_label();
tcg_gen_br(label30);
gen_set_label(label25);
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_8);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label30);
gen_set_label(label30);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState13qc_selectieqiE4BitsILj5ELb0EEhS1_h
static void emit_qc_selectieqi(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_8, uint8_t vi_12, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp17 = xqci_get_gpr(ctx, vi_4);
TCGLabel * label29 = gen_new_label();
TCGLabel * label30 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp17, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_19 << 3)) >> 3)), label29);
gen_set_label(label30);
tcg_gen_movi_i32(temp0, ((int32_t) ((vi_12 & 31) << 27) >> 27));
TCGLabel * label34 = gen_new_label();
tcg_gen_br(label34);
gen_set_label(label29);
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_8);
tcg_gen_mov_i32(temp0, temp6);
tcg_gen_br(label34);
gen_set_label(label34);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState13qc_selectiieqEh4BitsILj5ELb0EES1_h
static void emit_qc_selectiieq(DisasContext *ctx, TCGv_env env, uint8_t vi_16, uint8_t vi_9, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_setcond_i32(TCG_COND_EQ, temp7, temp13, temp14);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp7, tcg_constant_i32(0), tcg_constant_i32(vi_9), tcg_constant_i32(vi_11));
tcg_gen_andi_i32(temp7, temp0, 31);
tcg_gen_shli_i32(temp0, temp7, 27);
tcg_gen_sari_i32(temp0, temp0, 27);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState13qc_selectiineEh4BitsILj5ELb0EES1_h
static void emit_qc_selectiine(DisasContext *ctx, TCGv_env env, uint8_t vi_16, uint8_t vi_11, uint8_t vi_9, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp7 = tcg_temp_new_i32();
tcg_gen_setcond_i32(TCG_COND_EQ, temp7, temp13, temp14);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp7, tcg_constant_i32(0), tcg_constant_i32(vi_9), tcg_constant_i32(vi_11));
tcg_gen_andi_i32(temp7, temp0, 31);
tcg_gen_shli_i32(temp0, temp7, 27);
tcg_gen_sari_i32(temp0, temp0, 27);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState12qc_selectineEhh4BitsILj5ELb0EEh
static void emit_qc_selectine(DisasContext *ctx, TCGv_env env, uint8_t vi_21, uint8_t vi_14, uint8_t vi_9, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp19 = xqci_get_gpr(ctx, vi_21);
TCGLabel * label26 = gen_new_label();
TCGLabel * label27 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp18, temp19, label26);
gen_set_label(label27);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
tcg_gen_mov_i32(temp0, temp12);
TCGLabel * label28 = gen_new_label();
tcg_gen_br(label28);
gen_set_label(label26);
tcg_gen_movi_i32(temp0, ((int32_t) ((vi_9 & 31) << 27) >> 27));
tcg_gen_br(label28);
gen_set_label(label28);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState13qc_selectineiE4BitsILj5ELb0EEhS1_h
static void emit_qc_selectinei(DisasContext *ctx, TCGv_env env, uint8_t vi_20, uint8_t vi_14, uint8_t vi_9, uint8_t vi_4) {
TCGv_i32 temp0 = tcg_temp_new_i32();
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_4);
TCGLabel * label30 = gen_new_label();
TCGLabel * label31 = gen_new_label();
tcg_gen_brcondi_i32(TCG_COND_EQ, temp18, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_20 << 3)) >> 3)), label30);
gen_set_label(label31);
TCGv_i32 temp12 = xqci_get_gpr(ctx, vi_14);
tcg_gen_mov_i32(temp0, temp12);
TCGLabel * label32 = gen_new_label();
tcg_gen_br(label32);
gen_set_label(label30);
tcg_gen_movi_i32(temp0, ((int32_t) ((vi_9 & 31) << 27) >> 27));
tcg_gen_br(label32);
gen_set_label(label32);
tcg_gen_mov_i32(temp0, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState12qc_selectneiE4BitsILj5ELb0EEhhh
static void emit_qc_selectnei(DisasContext *ctx, TCGv_env env, uint8_t vi_16, uint8_t vi_10, uint8_t vi_12, uint8_t vi_4) {
TCGv_i32 temp14 = xqci_get_gpr(ctx, vi_4);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_setcondi_i32(TCG_COND_EQ, temp0, temp14, ((int32_t) (int32_t) ((int32_t) ((int32_t) (int8_t) (vi_16 << 3)) >> 3)));
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_12);
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_10);
tcg_gen_movcond_i32(TCG_COND_NE, temp0, temp0, tcg_constant_i32(0), temp6, temp7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_setintiE4BitsILj10ELb0EE
static void emit_qc_setinti(DisasContext *ctx, TCGv_env env, uint16_t vi_7) {
TCGv_i32 temp3 = xqci_csrr(ctx, env, ((((uint16_t) vi_7 >> 5) & 31) + 2032));
TCGv_i32 temp2 = tcg_temp_new_i32();
tcg_gen_ori_i32(temp2, temp3, (1 << (vi_7 & 31)));
xqci_csrw(ctx, env, ((((uint16_t) vi_7 >> 5) & 31) + 2032), temp2);
}

// void _ZN12CPUArchState9qc_shladdE4BitsILj5ELb0EEhhh
static void emit_qc_shladd(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_8, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp0, temp10, (vi_12 & 31));
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_8);
tcg_gen_add_i32(temp0, temp6, temp0);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState9qc_shlsatEhhh
static void emit_qc_shlsat(DisasContext *ctx, TCGv_env env, uint8_t vi_56, uint8_t vi_13, uint8_t vi_4) {
TCGv_i32 temp16 = xqci_get_gpr(ctx, vi_56);
TCGv_i32 temp34 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp34, temp16, 31);
TCGv_i32 temp53 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp53, temp16, tcg_constant_i32(0), tcg_constant_i32(3), tcg_constant_i32(0));
TCGv_i32 temp20 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp20, temp34, 2);
TCGv_i32 temp18 = tcg_temp_new_i32();
tcg_gen_or_i32(temp18, temp53, temp20);
TCGv_i32 temp19 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp19, temp34, 3);
tcg_gen_or_i32(temp19, temp18, temp19);
tcg_gen_shli_i32(temp20, temp34, 4);
tcg_gen_or_i32(temp20, temp19, temp20);
tcg_gen_shli_i32(temp18, temp34, 5);
tcg_gen_or_i32(temp18, temp20, temp18);
tcg_gen_shli_i32(temp19, temp34, 6);
TCGv_i32 temp17 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp17, temp34, 13);
TCGv_i32 temp31 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp31, temp34, 14);
TCGv_i32 temp30 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp30, temp34, 15);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp0, temp34, 16);
TCGv_i32 temp26 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp26, temp34, 23);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp10, temp34, 24);
TCGv_i32 temp25 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp25, temp34, 25);
TCGv_i32 temp24 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp24, temp34, 26);
TCGv_i32 temp23 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp23, temp34, 27);
TCGv_i32 temp22 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp22, temp34, 28);
TCGv_i32 temp21 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp21, temp34, 29);
tcg_gen_shli_i32(temp20, temp34, 30);
TCGv_i32 temp32 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp32, temp16, INT32_MIN);
tcg_gen_movcond_i32(TCG_COND_LT, temp34, temp16, tcg_constant_i32(0), tcg_constant_i32(1920), tcg_constant_i32(0));
tcg_gen_or_i32(temp32, temp19, temp32);
tcg_gen_or_i32(temp19, temp32, temp34);
tcg_gen_movcond_i32(TCG_COND_LT, temp32, temp16, tcg_constant_i32(0), tcg_constant_i32(6144), tcg_constant_i32(0));
tcg_gen_or_i32(temp17, temp32, temp17);
tcg_gen_or_i32(temp17, temp17, temp31);
tcg_gen_or_i32(temp17, temp17, temp30);
tcg_gen_or_i32(temp0, temp17, temp0);
tcg_gen_movcond_i32(TCG_COND_LT, temp17, temp16, tcg_constant_i32(0), tcg_constant_i32(8257536), tcg_constant_i32(0));
tcg_gen_or_i32(temp10, temp10, temp26);
tcg_gen_or_i32(temp10, temp10, temp25);
tcg_gen_or_i32(temp10, temp10, temp24);
tcg_gen_or_i32(temp10, temp10, temp23);
tcg_gen_or_i32(temp10, temp10, temp22);
tcg_gen_or_i32(temp10, temp10, temp21);
tcg_gen_or_i32(temp10, temp10, temp20);
tcg_gen_or_i32(temp10, temp10, temp19);
tcg_gen_or_i32(temp10, temp10, temp18);
tcg_gen_or_i32(temp0, temp10, temp0);
tcg_gen_or_i32(temp10, temp0, temp17);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp10);
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp16);
TCGv_i64 temp7 = tcg_temp_new_i64();
tcg_gen_shli_i64(temp7, temp3, 32ull);
tcg_gen_or_i64(temp6, temp7, temp6);
temp10 = xqci_get_gpr(ctx, vi_13);
tcg_gen_andi_i32(temp0, temp10, 31);
tcg_gen_extu_i32_i64(temp3, temp0);
tcg_gen_shl_i64(temp3, temp6, temp3);
tcg_gen_smin_i64(temp7, temp3, tcg_constant_i64(2147483647ull));
tcg_gen_smax_i64(temp6, temp7, tcg_constant_i64(-2147483648ll));
tcg_gen_extrl_i64_i32(temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_shlusatEhhh
static void emit_qc_shlusat(DisasContext *ctx, TCGv_env env, uint8_t vi_15, uint8_t vi_11, uint8_t vi_4) {
TCGv_i32 temp13 = xqci_get_gpr(ctx, vi_15);
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp13);
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_11);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_andi_i32(temp0, temp8, 31);
TCGv_i64 temp3 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp3, temp0);
tcg_gen_shl_i64(temp3, temp6, temp3);
tcg_gen_umin_i64(temp6, temp3, tcg_constant_i64(4294967295ull));
tcg_gen_extrl_i64_i32(temp0, temp6);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState6qc_srbE4BitsILj3ELb0EEhhh
static void emit_qc_srb(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_19, uint8_t vi_16, uint8_t vi_7) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp1 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp1, temp10, (vi_12 & 7));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp1, temp9);
TCGv_i32 temp4 = xqci_get_gpr(ctx, vi_7);
tcg_gen_andi_i32(temp1, temp4, 255);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_UB);
}

// void _ZN12CPUArchState6qc_srhE4BitsILj3ELb0EEhhh
static void emit_qc_srh(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_19, uint8_t vi_16, uint8_t vi_7) {
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_19);
TCGv_i32 temp10 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp1 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp1, temp10, (vi_12 & 7));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp1, temp9);
TCGv_i32 temp4 = xqci_get_gpr(ctx, vi_7);
tcg_gen_andi_i32(temp1, temp4, 65535);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_LEUW);
}

// void _ZN12CPUArchState6qc_srwE4BitsILj3ELb0EEhhh
static void emit_qc_srw(DisasContext *ctx, TCGv_env env, uint8_t vi_10, uint8_t vi_17, uint8_t vi_14, uint8_t vi_5) {
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_17);
TCGv_i32 temp8 = xqci_get_gpr(ctx, vi_14);
TCGv_i32 temp1 = tcg_temp_new_i32();
tcg_gen_shli_i32(temp1, temp8, (vi_10 & 7));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp1, temp7);
temp1 = xqci_get_gpr(ctx, vi_5);
tcg_gen_qemu_st_i32(temp1, temp0, ctx->mem_idx, MO_LEUL);
}

// void _ZN12CPUArchState9qc_subsatEhhh
static void emit_qc_subsat(DisasContext *ctx, TCGv_env env, uint8_t vi_24, uint8_t vi_21, uint8_t vi_4) {
TCGv_i64 temp3 = tcg_temp_new_i64();
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_24);
TCGv_i32 temp18 = xqci_get_gpr(ctx, vi_21);
TCGv_i64 temp15 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp15, temp9);
TCGv_i64 temp6 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp6, temp18);
tcg_gen_sub_i64(temp6, temp15, temp6);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp0, temp9, 31);
TCGv_i32 temp13 = tcg_temp_new_i32();
tcg_gen_shri_i32(temp13, temp18, 31);
tcg_gen_mov_i64(temp3, temp6);
TCGLabel * label28 = gen_new_label();
TCGLabel * label29 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp13, temp0, label28);
gen_set_label(label29);
tcg_gen_extract_i32(temp13, temp0, 0, 8);
tcg_gen_shri_i64(temp15, temp6, 31ull);
TCGv_i64 temp34 = tcg_temp_new_i64();
tcg_gen_extract_i64(temp34, temp15, 0, 8);
tcg_gen_extrl_i64_i32(temp0, temp34);
tcg_gen_andi_i32(temp0, temp0, 1);
tcg_gen_mov_i64(temp3, temp6);
TCGLabel * label35 = gen_new_label();
tcg_gen_brcond_i32(TCG_COND_EQ, temp0, temp13, label28);
gen_set_label(label35);
tcg_gen_setcondi_i32(TCG_COND_LT, temp0, temp9, 0);
TCGv_i64 temp37 = tcg_temp_new_i64();
tcg_gen_extu_i32_i64(temp37, temp0);
tcg_gen_movcond_i64(TCG_COND_NE, temp6, temp37, tcg_constant_i64(0), tcg_constant_i64(6442450944ull), tcg_constant_i64(2147483647ull));
tcg_gen_mov_i64(temp3, temp6);
tcg_gen_br(label28);
gen_set_label(label28);
tcg_gen_mov_i64(temp3, temp3);
tcg_gen_extrl_i64_i32(temp0, temp3);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState10qc_subusatEhhh
static void emit_qc_subusat(DisasContext *ctx, TCGv_env env, uint8_t vi_12, uint8_t vi_9, uint8_t vi_4) {
TCGv_i32 temp6 = xqci_get_gpr(ctx, vi_12);
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_9);
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_ussub_i32(temp0, temp6, temp7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState7qc_wrapEhhh
static void emit_qc_wrap(DisasContext *ctx, TCGv_env env, uint8_t vi_16, uint8_t vi_13, uint8_t vi_4) {
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp9 = xqci_get_gpr(ctx, vi_13);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp10, temp7, tcg_constant_i32(0), temp9, tcg_constant_i32(0));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_sub_i32(temp0, tcg_constant_i32(0), temp9);
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp6, temp7, temp9, temp10, temp0);
tcg_gen_add_i32(temp0, temp6, temp7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

// void _ZN12CPUArchState8qc_wrapiE4BitsILj11ELb0EEhh
static void emit_qc_wrapi(DisasContext *ctx, TCGv_env env, uint16_t vi_12, uint8_t vi_16, uint8_t vi_4) {
TCGv_i32 temp7 = xqci_get_gpr(ctx, vi_16);
TCGv_i32 temp10 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp10, temp7, tcg_constant_i32(0), tcg_constant_i32((vi_12 & 2047)), tcg_constant_i32(0));
TCGv_i32 temp6 = tcg_temp_new_i32();
tcg_gen_movcond_i32(TCG_COND_LT, temp6, temp7, tcg_constant_i32((vi_12 & 2047)), temp10, tcg_constant_i32((0 - (vi_12 & 2047))));
TCGv_i32 temp0 = tcg_temp_new_i32();
tcg_gen_add_i32(temp0, temp6, temp7);
tcg_gen_mov_i32(cpu_gpr[((uint64_t) (uint32_t) vi_4)], temp0);
}

