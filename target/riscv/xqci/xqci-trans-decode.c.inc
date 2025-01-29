static bool trans_qc_addsat(DisasContext *ctx, arg_qc_addsat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_addsat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_addusat(DisasContext *ctx, arg_qc_addusat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_addusat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_beqi(DisasContext *ctx, arg_qc_beqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_beqi(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_bgei(DisasContext *ctx, arg_qc_bgei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_bgei(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_bgeui(DisasContext *ctx, arg_qc_bgeui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_bgeui(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_blti(DisasContext *ctx, arg_qc_blti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_blti(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_bltui(DisasContext *ctx, arg_qc_bltui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_bltui(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_bnei(DisasContext *ctx, arg_qc_bnei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_bnei(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_brev32(DisasContext *ctx, arg_qc_brev32 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_brev32(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_bexti(DisasContext *ctx, arg_qc_c_bexti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->shamt == 0) {
        return false;
    }
    emit_qc_c_bexti(ctx, tcg_env, arg->shamt, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_bseti(DisasContext *ctx, arg_qc_c_bseti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->shamt == 0) {
        return false;
    }
    emit_qc_c_bseti(ctx, tcg_env, arg->shamt, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_clrint(DisasContext *ctx, arg_qc_c_clrint *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_c_clrint(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_delay(DisasContext *ctx, arg_qc_c_delay *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    if (arg->imm == 0) {
        return false;
    }
    emit_qc_c_delay(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_di(DisasContext *ctx, arg_qc_c_di *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_di(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_dir(DisasContext *ctx, arg_qc_c_dir *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_c_dir(ctx, tcg_env, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_ei(DisasContext *ctx, arg_qc_c_ei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_ei(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_eir(DisasContext *ctx, arg_qc_c_eir *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_c_eir(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_extu(DisasContext *ctx, arg_qc_c_extu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->width_minus1 == 0 || arg->width_minus1 == 1 || arg->width_minus1 == 2 || arg->width_minus1 == 3 || arg->width_minus1 == 4) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_c_extu(ctx, tcg_env, arg->width_minus1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_mienter_nest(DisasContext *ctx, arg_qc_c_mienter_nest *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_mienter_nest(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_mienter(DisasContext *ctx, arg_qc_c_mienter *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_mienter(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_mileaveret(DisasContext *ctx, arg_qc_c_mileaveret *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_mileaveret(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_mnret(DisasContext *ctx, arg_qc_c_mnret *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_mnret(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_mret(DisasContext *ctx, arg_qc_c_mret *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_c_mret(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_muliadd(DisasContext *ctx, arg_qc_c_muliadd *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciac) {
        return false;
    }
    emit_qc_c_muliadd(ctx, tcg_env, arg->uimm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_mveqz(DisasContext *ctx, arg_qc_c_mveqz *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    emit_qc_c_mveqz(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_ptrace(DisasContext *ctx, arg_qc_c_ptrace *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_c_ptrace(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_setint(DisasContext *ctx, arg_qc_c_setint *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_c_setint(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_c_sync(DisasContext *ctx, arg_qc_c_sync *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_c_sync(ctx, tcg_env, arg->slist);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_syncr(DisasContext *ctx, arg_qc_c_syncr *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_c_syncr(ctx, tcg_env, arg->slist);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_syncwf(DisasContext *ctx, arg_qc_c_syncwf *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_c_syncwf(ctx, tcg_env, arg->slist);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_c_syncwl(DisasContext *ctx, arg_qc_c_syncwl *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_c_syncwl(ctx, tcg_env, arg->slist);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_clo(DisasContext *ctx, arg_qc_clo *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_clo(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_clrinti(DisasContext *ctx, arg_qc_clrinti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_clrinti(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_compress2(DisasContext *ctx, arg_qc_compress2 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_compress2(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_compress3(DisasContext *ctx, arg_qc_compress3 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_compress3(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_csrrwr(DisasContext *ctx, arg_qc_csrrwr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicsr) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_csrrwr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_csrrwri(DisasContext *ctx, arg_qc_csrrwri *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicsr) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_csrrwri(ctx, tcg_env, arg->rs2, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cto(DisasContext *ctx, arg_qc_cto *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_cto(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_addai(DisasContext *ctx, arg_qc_e_addai *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_addai(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_addi(DisasContext *ctx, arg_qc_e_addi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_addi(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_andai(DisasContext *ctx, arg_qc_e_andai *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_andai(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_andi(DisasContext *ctx, arg_qc_e_andi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_andi(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_beqi(DisasContext *ctx, arg_qc_e_beqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_beqi(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_bgei(DisasContext *ctx, arg_qc_e_bgei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_bgei(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_bgeui(DisasContext *ctx, arg_qc_e_bgeui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_bgeui(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_blti(DisasContext *ctx, arg_qc_e_blti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_blti(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_bltui(DisasContext *ctx, arg_qc_e_bltui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_bltui(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_bnei(DisasContext *ctx, arg_qc_e_bnei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    emit_qc_e_bnei(ctx, tcg_env, arg->offset, arg->imm, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_j(DisasContext *ctx, arg_qc_e_j *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilb) {
        return false;
    }
    arg->imm <<= 1;
    emit_qc_e_j(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_jal(DisasContext *ctx, arg_qc_e_jal *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilb) {
        return false;
    }
    arg->imm <<= 1;
    emit_qc_e_jal(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_lb(DisasContext *ctx, arg_qc_e_lb *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_lb(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_lbu(DisasContext *ctx, arg_qc_e_lbu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_lbu(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_lh(DisasContext *ctx, arg_qc_e_lh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_lh(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_lhu(DisasContext *ctx, arg_qc_e_lhu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_lhu(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_li(DisasContext *ctx, arg_qc_e_li *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcili) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_li(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_lw(DisasContext *ctx, arg_qc_e_lw *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_lw(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_orai(DisasContext *ctx, arg_qc_e_orai *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_orai(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_ori(DisasContext *ctx, arg_qc_e_ori *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_ori(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_sb(DisasContext *ctx, arg_qc_e_sb *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_sb(ctx, tcg_env, arg->imm, arg->rs2, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_sh(DisasContext *ctx, arg_qc_e_sh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_sh(ctx, tcg_env, arg->imm, arg->rs2, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_sw(DisasContext *ctx, arg_qc_e_sw *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilo) {
        return false;
    }
    emit_qc_e_sw(ctx, tcg_env, arg->imm, arg->rs2, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_xorai(DisasContext *ctx, arg_qc_e_xorai *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_xorai(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_e_xori(DisasContext *ctx, arg_qc_e_xori *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_e_xori(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_expand2(DisasContext *ctx, arg_qc_expand2 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_expand2(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_expand3(DisasContext *ctx, arg_qc_expand3 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_expand3(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ext(DisasContext *ctx, arg_qc_ext *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_ext(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extd(DisasContext *ctx, arg_qc_extd *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extd(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdpr(DisasContext *ctx, arg_qc_extdpr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdpr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdprh(DisasContext *ctx, arg_qc_extdprh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdprh(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdr(DisasContext *ctx, arg_qc_extdr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdu(DisasContext *ctx, arg_qc_extdu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdu(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdupr(DisasContext *ctx, arg_qc_extdupr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdupr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extduprh(DisasContext *ctx, arg_qc_extduprh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extduprh(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extdur(DisasContext *ctx, arg_qc_extdur *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extdur(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_extu(DisasContext *ctx, arg_qc_extu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_extu(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insb(DisasContext *ctx, arg_qc_insb *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insb(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbh(DisasContext *ctx, arg_qc_insbh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbh(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbhr(DisasContext *ctx, arg_qc_insbhr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbhr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbi(DisasContext *ctx, arg_qc_insbi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbi(ctx, tcg_env, arg->width_minus1, arg->shamt, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbpr(DisasContext *ctx, arg_qc_insbpr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbpr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbprh(DisasContext *ctx, arg_qc_insbprh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbprh(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbr(DisasContext *ctx, arg_qc_insbr *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbr(ctx, tcg_env, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_insbri(DisasContext *ctx, arg_qc_insbri *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_insbri(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_inw(DisasContext *ctx, arg_qc_inw *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciio) {
        return false;
    }
    arg->imm <<= 2;
    emit_qc_inw(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_li(DisasContext *ctx, arg_qc_li *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcili) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_li(ctx, tcg_env, arg->imm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lieq(DisasContext *ctx, arg_qc_lieq *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lieq(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lieqi(DisasContext *ctx, arg_qc_lieqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lieqi(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lige(DisasContext *ctx, arg_qc_lige *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lige(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ligei(DisasContext *ctx, arg_qc_ligei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_ligei(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ligeu(DisasContext *ctx, arg_qc_ligeu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_ligeu(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ligeui(DisasContext *ctx, arg_qc_ligeui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_ligeui(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lilt(DisasContext *ctx, arg_qc_lilt *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lilt(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lilti(DisasContext *ctx, arg_qc_lilti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lilti(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_liltu(DisasContext *ctx, arg_qc_liltu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_liltu(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_liltui(DisasContext *ctx, arg_qc_liltui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_liltui(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_line(DisasContext *ctx, arg_qc_line *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_line(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_linei(DisasContext *ctx, arg_qc_linei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_linei(ctx, tcg_env, arg->rs1, arg->imm, arg->simm, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lrb(DisasContext *ctx, arg_qc_lrb *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_lrb(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lrbu(DisasContext *ctx, arg_qc_lrbu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_lrbu(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lrh(DisasContext *ctx, arg_qc_lrh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_lrh(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lrhu(DisasContext *ctx, arg_qc_lrhu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_lrhu(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lrw(DisasContext *ctx, arg_qc_lrw *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_lrw(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lwm(DisasContext *ctx, arg_qc_lwm *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lwm(ctx, tcg_env, arg->imm, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_lwmi(DisasContext *ctx, arg_qc_lwmi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_lwmi(ctx, tcg_env, arg->imm, arg->rs1, arg->length, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_muliadd(DisasContext *ctx, arg_qc_muliadd *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciac) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_muliadd(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mveq(DisasContext *ctx, arg_qc_mveq *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mveq(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mveqi(DisasContext *ctx, arg_qc_mveqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mveqi(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvge(DisasContext *ctx, arg_qc_mvge *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvge(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvgei(DisasContext *ctx, arg_qc_mvgei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvgei(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvgeu(DisasContext *ctx, arg_qc_mvgeu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvgeu(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvgeui(DisasContext *ctx, arg_qc_mvgeui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvgeui(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvlt(DisasContext *ctx, arg_qc_mvlt *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvlt(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvlti(DisasContext *ctx, arg_qc_mvlti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvlti(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvltu(DisasContext *ctx, arg_qc_mvltu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvltu(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvltui(DisasContext *ctx, arg_qc_mvltui *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvltui(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvne(DisasContext *ctx, arg_qc_mvne *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvne(ctx, tcg_env, arg->rs1, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_mvnei(DisasContext *ctx, arg_qc_mvnei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcicm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_mvnei(ctx, tcg_env, arg->rs1, arg->imm, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_norm(DisasContext *ctx, arg_qc_norm *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_norm(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_normeu(DisasContext *ctx, arg_qc_normeu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_normeu(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_normu(DisasContext *ctx, arg_qc_normu *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_normu(ctx, tcg_env, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_outw(DisasContext *ctx, arg_qc_outw *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciio) {
        return false;
    }
    arg->imm <<= 2;
    emit_qc_outw(ctx, tcg_env, arg->imm, arg->rs1, arg->rs2);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_pcoredump(DisasContext *ctx, arg_qc_pcoredump *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_pcoredump(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_pexit(DisasContext *ctx, arg_qc_pexit *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_pexit(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ppreg(DisasContext *ctx, arg_qc_ppreg *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_ppreg(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_ppregs(DisasContext *ctx, arg_qc_ppregs *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_ppregs(ctx, tcg_env);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_pputc(DisasContext *ctx, arg_qc_pputc *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_pputc(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_pputci(DisasContext *ctx, arg_qc_pputci *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_pputci(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_pputs(DisasContext *ctx, arg_qc_pputs *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_pputs(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_psyscall(DisasContext *ctx, arg_qc_psyscall *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_psyscall(ctx, tcg_env, arg->rs1);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_psyscalli(DisasContext *ctx, arg_qc_psyscalli *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisim) {
        return false;
    }
    emit_qc_psyscalli(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selecteqi(DisasContext *ctx, arg_qc_selecteqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selecteqi(ctx, tcg_env, arg->imm, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectieq(DisasContext *ctx, arg_qc_selectieq *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectieq(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectieqi(DisasContext *ctx, arg_qc_selectieqi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectieqi(ctx, tcg_env, arg->imm, arg->rs2, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectiieq(DisasContext *ctx, arg_qc_selectiieq *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectiieq(ctx, tcg_env, arg->rs1, arg->simm1, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectiine(DisasContext *ctx, arg_qc_selectiine *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectiine(ctx, tcg_env, arg->rs1, arg->simm1, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectine(DisasContext *ctx, arg_qc_selectine *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectine(ctx, tcg_env, arg->rs1, arg->rs2, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectinei(DisasContext *ctx, arg_qc_selectinei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectinei(ctx, tcg_env, arg->imm, arg->rs2, arg->simm2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_selectnei(DisasContext *ctx, arg_qc_selectnei *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_selectnei(ctx, tcg_env, arg->imm, arg->rs2, arg->rs3, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_setinti(DisasContext *ctx, arg_qc_setinti *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciint) {
        return false;
    }
    emit_qc_setinti(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_setwm(DisasContext *ctx, arg_qc_setwm *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_setwm(ctx, tcg_env, arg->imm, arg->rs1, arg->rs2, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_setwmi(DisasContext *ctx, arg_qc_setwmi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    emit_qc_setwmi(ctx, tcg_env, arg->imm, arg->rs1, arg->length, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_shladd(DisasContext *ctx, arg_qc_shladd *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqciac) {
        return false;
    }
    if (arg->shamt == 0 || arg->shamt == 1 || arg->shamt == 2 || arg->shamt == 3) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_shladd(ctx, tcg_env, arg->shamt, arg->rs2, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_shlsat(DisasContext *ctx, arg_qc_shlsat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_shlsat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_shlusat(DisasContext *ctx, arg_qc_shlusat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_shlusat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_srb(DisasContext *ctx, arg_qc_srb *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_srb(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_srh(DisasContext *ctx, arg_qc_srh *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_srh(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_srw(DisasContext *ctx, arg_qc_srw *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    emit_qc_srw(ctx, tcg_env, arg->shamt, arg->rs1, arg->rs2, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_subsat(DisasContext *ctx, arg_qc_subsat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_subsat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_subusat(DisasContext *ctx, arg_qc_subusat *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_subusat(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_swm(DisasContext *ctx, arg_qc_swm *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    emit_qc_swm(ctx, tcg_env, arg->imm, arg->rs1, arg->rs2, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_swmi(DisasContext *ctx, arg_qc_swmi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    emit_qc_swmi(ctx, tcg_env, arg->imm, arg->rs1, arg->length, arg->rs3);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_sync(DisasContext *ctx, arg_qc_sync *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_sync(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_syncr(DisasContext *ctx, arg_qc_syncr *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_syncr(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_syncwf(DisasContext *ctx, arg_qc_syncwf *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_syncwf(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_syncwl(DisasContext *ctx, arg_qc_syncwl *arg)
{
#if !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcisync) {
        return false;
    }
    emit_qc_syncwl(ctx, tcg_env, arg->imm);
    return true;
#endif /* !defined(TARGET_RISCV32) || defined(CONFIG_USER_ONLY) */
}
static bool trans_qc_wrap(DisasContext *ctx, arg_qc_wrap *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_wrap(ctx, tcg_env, arg->rs1, arg->rs2, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_wrapi(DisasContext *ctx, arg_qc_wrapi *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqci && !ctx->cfg_ptr->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    emit_qc_wrapi(ctx, tcg_env, arg->imm, arg->rs1, arg->rd);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
