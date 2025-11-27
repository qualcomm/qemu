static bool trans_qc_cm_mva01s(DisasContext *ctx, arg_qc_cm_mva01s *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    emit_qc_cm_mva01s(ctx, tcg_env, arg->r1s, arg->r2s);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_mvsa01(DisasContext *ctx, arg_qc_cm_mvsa01 *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    emit_qc_cm_mvsa01(ctx, tcg_env, arg->r1s, arg->r2s);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_pop(DisasContext *ctx, arg_qc_cm_pop *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    emit_qc_cm_pop(ctx, tcg_env, arg->rlist, arg->spimm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_popret(DisasContext *ctx, arg_qc_cm_popret *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    emit_qc_cm_popret(ctx, tcg_env, arg->rlist, arg->spimm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_popretz(DisasContext *ctx, arg_qc_cm_popretz *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    emit_qc_cm_popretz(ctx, tcg_env, arg->rlist, arg->spimm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_push(DisasContext *ctx, arg_qc_cm_push *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    emit_qc_cm_push(ctx, tcg_env, arg->rlist, arg->spimm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
static bool trans_qc_cm_pushfp(DisasContext *ctx, arg_qc_cm_pushfp *arg)
{
#if !defined(TARGET_RISCV32)
    return false;
#else
    if (!ctx->cfg_ptr->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3 || arg->rlist == 4) {
        return false;
    }
    arg->spimm <<= 4;
    emit_qc_cm_pushfp(ctx, tcg_env, arg->rlist, arg->spimm);
    return true;
#endif /* !defined(TARGET_RISCV32) */
}
