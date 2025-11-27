#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_vendorid.h"
#include "xqci-csr.h"

static RISCVException rmw_qc_mcause(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mcause;
    }
    env->qc_mcause = (env->qc_mcause & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mcause(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie0(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie0;
    }
    env->qc_mclicie0 = (env->qc_mclicie0 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie0(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie1(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie1;
    }
    env->qc_mclicie1 = (env->qc_mclicie1 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie1(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie2(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie2;
    }
    env->qc_mclicie2 = (env->qc_mclicie2 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie2(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie3(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie3;
    }
    env->qc_mclicie3 = (env->qc_mclicie3 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie3(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie4(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie4;
    }
    env->qc_mclicie4 = (env->qc_mclicie4 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie4(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie5(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie5;
    }
    env->qc_mclicie5 = (env->qc_mclicie5 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie5(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie6(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie6;
    }
    env->qc_mclicie6 = (env->qc_mclicie6 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie6(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicie7(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicie7;
    }
    env->qc_mclicie7 = (env->qc_mclicie7 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicie7(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl00(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl00;
    }
    env->qc_mclicilvl00 = (env->qc_mclicilvl00 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl00(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl01(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl01;
    }
    env->qc_mclicilvl01 = (env->qc_mclicilvl01 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl01(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl02(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl02;
    }
    env->qc_mclicilvl02 = (env->qc_mclicilvl02 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl02(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl03(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl03;
    }
    env->qc_mclicilvl03 = (env->qc_mclicilvl03 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl03(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl04(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl04;
    }
    env->qc_mclicilvl04 = (env->qc_mclicilvl04 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl04(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl05(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl05;
    }
    env->qc_mclicilvl05 = (env->qc_mclicilvl05 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl05(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl06(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl06;
    }
    env->qc_mclicilvl06 = (env->qc_mclicilvl06 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl06(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl07(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl07;
    }
    env->qc_mclicilvl07 = (env->qc_mclicilvl07 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl07(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl08(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl08;
    }
    env->qc_mclicilvl08 = (env->qc_mclicilvl08 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl08(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl09(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl09;
    }
    env->qc_mclicilvl09 = (env->qc_mclicilvl09 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl09(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl10(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl10;
    }
    env->qc_mclicilvl10 = (env->qc_mclicilvl10 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl10(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl11(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl11;
    }
    env->qc_mclicilvl11 = (env->qc_mclicilvl11 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl11(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl12(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl12;
    }
    env->qc_mclicilvl12 = (env->qc_mclicilvl12 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl12(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl13(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl13;
    }
    env->qc_mclicilvl13 = (env->qc_mclicilvl13 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl13(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl14(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl14;
    }
    env->qc_mclicilvl14 = (env->qc_mclicilvl14 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl14(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl15(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl15;
    }
    env->qc_mclicilvl15 = (env->qc_mclicilvl15 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl15(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl16(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl16;
    }
    env->qc_mclicilvl16 = (env->qc_mclicilvl16 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl16(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl17(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl17;
    }
    env->qc_mclicilvl17 = (env->qc_mclicilvl17 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl17(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl18(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl18;
    }
    env->qc_mclicilvl18 = (env->qc_mclicilvl18 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl18(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl19(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl19;
    }
    env->qc_mclicilvl19 = (env->qc_mclicilvl19 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl19(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl20(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl20;
    }
    env->qc_mclicilvl20 = (env->qc_mclicilvl20 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl20(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl21(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl21;
    }
    env->qc_mclicilvl21 = (env->qc_mclicilvl21 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl21(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl22(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl22;
    }
    env->qc_mclicilvl22 = (env->qc_mclicilvl22 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl22(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl23(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl23;
    }
    env->qc_mclicilvl23 = (env->qc_mclicilvl23 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl23(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl24(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl24;
    }
    env->qc_mclicilvl24 = (env->qc_mclicilvl24 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl24(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl25(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl25;
    }
    env->qc_mclicilvl25 = (env->qc_mclicilvl25 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl25(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl26(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl26;
    }
    env->qc_mclicilvl26 = (env->qc_mclicilvl26 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl26(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl27(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl27;
    }
    env->qc_mclicilvl27 = (env->qc_mclicilvl27 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl27(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl28(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl28;
    }
    env->qc_mclicilvl28 = (env->qc_mclicilvl28 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl28(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl29(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl29;
    }
    env->qc_mclicilvl29 = (env->qc_mclicilvl29 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl29(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl30(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl30;
    }
    env->qc_mclicilvl30 = (env->qc_mclicilvl30 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl30(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicilvl31(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicilvl31;
    }
    env->qc_mclicilvl31 = (env->qc_mclicilvl31 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicilvl31(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip0(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip0;
    }
    env->qc_mclicip0 = (env->qc_mclicip0 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip0(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip1(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip1;
    }
    env->qc_mclicip1 = (env->qc_mclicip1 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip1(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip2(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip2;
    }
    env->qc_mclicip2 = (env->qc_mclicip2 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip2(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip3(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip3;
    }
    env->qc_mclicip3 = (env->qc_mclicip3 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip3(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip4(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip4;
    }
    env->qc_mclicip4 = (env->qc_mclicip4 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip4(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip5(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip5;
    }
    env->qc_mclicip5 = (env->qc_mclicip5 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip5(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip6(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip6;
    }
    env->qc_mclicip6 = (env->qc_mclicip6 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip6(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mclicip7(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mclicip7;
    }
    env->qc_mclicip7 = (env->qc_mclicip7 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mclicip7(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mmcr(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mmcr;
    }
    env->qc_mmcr = (env->qc_mmcr & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mmcr(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mntvec(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mntvec;
    }
    env->qc_mntvec = (env->qc_mntvec & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mntvec(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mstkbottomaddr(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mstkbottomaddr;
    }
    env->qc_mstkbottomaddr = (env->qc_mstkbottomaddr & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mstkbottomaddr(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mstktopaddr(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mstktopaddr;
    }
    env->qc_mstktopaddr = (env->qc_mstktopaddr & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mstktopaddr(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mthreadptr(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mthreadptr;
    }
    env->qc_mthreadptr = (env->qc_mthreadptr & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mthreadptr(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpendaddr0(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpendaddr0;
    }
    env->qc_mwpendaddr0 = (env->qc_mwpendaddr0 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpendaddr0(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpendaddr1(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpendaddr1;
    }
    env->qc_mwpendaddr1 = (env->qc_mwpendaddr1 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpendaddr1(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpendaddr2(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpendaddr2;
    }
    env->qc_mwpendaddr2 = (env->qc_mwpendaddr2 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpendaddr2(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpendaddr3(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpendaddr3;
    }
    env->qc_mwpendaddr3 = (env->qc_mwpendaddr3 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpendaddr3(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpstartaddr0(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpstartaddr0;
    }
    env->qc_mwpstartaddr0 = (env->qc_mwpstartaddr0 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpstartaddr0(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpstartaddr1(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpstartaddr1;
    }
    env->qc_mwpstartaddr1 = (env->qc_mwpstartaddr1 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpstartaddr1(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpstartaddr2(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpstartaddr2;
    }
    env->qc_mwpstartaddr2 = (env->qc_mwpstartaddr2 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpstartaddr2(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
static RISCVException rmw_qc_mwpstartaddr3(CPURISCVState * env,
                              int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val,
                              target_ulong wr_mask)
{
    if (ret_val) {
        *ret_val = env->qc_mwpstartaddr3;
    }
    env->qc_mwpstartaddr3 = (env->qc_mwpstartaddr3 & ~wr_mask) | (new_val & wr_mask);
    return RISCV_EXCP_NONE;
}
static RISCVException pred_qc_mwpstartaddr3(CPURISCVState * env,
                               int csrno)
{
    if (env->priv != PRV_M || (!riscv_cpu_cfg(env)->ext_xqci && !riscv_cpu_cfg(env)->ext_xqciint)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}

const RISCVCSR xqci_csr_list[] = {
    {
        .csrno = CSR_QC_MCAUSE,
        .csr_ops = {"qc_mcause", pred_qc_mcause, NULL, NULL, rmw_qc_mcause},
    },
    {
        .csrno = CSR_QC_MCLICIE0,
        .csr_ops = {"qc_mclicie0", pred_qc_mclicie0, NULL, NULL, rmw_qc_mclicie0},
    },
    {
        .csrno = CSR_QC_MCLICIE1,
        .csr_ops = {"qc_mclicie1", pred_qc_mclicie1, NULL, NULL, rmw_qc_mclicie1},
    },
    {
        .csrno = CSR_QC_MCLICIE2,
        .csr_ops = {"qc_mclicie2", pred_qc_mclicie2, NULL, NULL, rmw_qc_mclicie2},
    },
    {
        .csrno = CSR_QC_MCLICIE3,
        .csr_ops = {"qc_mclicie3", pred_qc_mclicie3, NULL, NULL, rmw_qc_mclicie3},
    },
    {
        .csrno = CSR_QC_MCLICIE4,
        .csr_ops = {"qc_mclicie4", pred_qc_mclicie4, NULL, NULL, rmw_qc_mclicie4},
    },
    {
        .csrno = CSR_QC_MCLICIE5,
        .csr_ops = {"qc_mclicie5", pred_qc_mclicie5, NULL, NULL, rmw_qc_mclicie5},
    },
    {
        .csrno = CSR_QC_MCLICIE6,
        .csr_ops = {"qc_mclicie6", pred_qc_mclicie6, NULL, NULL, rmw_qc_mclicie6},
    },
    {
        .csrno = CSR_QC_MCLICIE7,
        .csr_ops = {"qc_mclicie7", pred_qc_mclicie7, NULL, NULL, rmw_qc_mclicie7},
    },
    {
        .csrno = CSR_QC_MCLICILVL00,
        .csr_ops = {"qc_mclicilvl00", pred_qc_mclicilvl00, NULL, NULL, rmw_qc_mclicilvl00},
    },
    {
        .csrno = CSR_QC_MCLICILVL01,
        .csr_ops = {"qc_mclicilvl01", pred_qc_mclicilvl01, NULL, NULL, rmw_qc_mclicilvl01},
    },
    {
        .csrno = CSR_QC_MCLICILVL02,
        .csr_ops = {"qc_mclicilvl02", pred_qc_mclicilvl02, NULL, NULL, rmw_qc_mclicilvl02},
    },
    {
        .csrno = CSR_QC_MCLICILVL03,
        .csr_ops = {"qc_mclicilvl03", pred_qc_mclicilvl03, NULL, NULL, rmw_qc_mclicilvl03},
    },
    {
        .csrno = CSR_QC_MCLICILVL04,
        .csr_ops = {"qc_mclicilvl04", pred_qc_mclicilvl04, NULL, NULL, rmw_qc_mclicilvl04},
    },
    {
        .csrno = CSR_QC_MCLICILVL05,
        .csr_ops = {"qc_mclicilvl05", pred_qc_mclicilvl05, NULL, NULL, rmw_qc_mclicilvl05},
    },
    {
        .csrno = CSR_QC_MCLICILVL06,
        .csr_ops = {"qc_mclicilvl06", pred_qc_mclicilvl06, NULL, NULL, rmw_qc_mclicilvl06},
    },
    {
        .csrno = CSR_QC_MCLICILVL07,
        .csr_ops = {"qc_mclicilvl07", pred_qc_mclicilvl07, NULL, NULL, rmw_qc_mclicilvl07},
    },
    {
        .csrno = CSR_QC_MCLICILVL08,
        .csr_ops = {"qc_mclicilvl08", pred_qc_mclicilvl08, NULL, NULL, rmw_qc_mclicilvl08},
    },
    {
        .csrno = CSR_QC_MCLICILVL09,
        .csr_ops = {"qc_mclicilvl09", pred_qc_mclicilvl09, NULL, NULL, rmw_qc_mclicilvl09},
    },
    {
        .csrno = CSR_QC_MCLICILVL10,
        .csr_ops = {"qc_mclicilvl10", pred_qc_mclicilvl10, NULL, NULL, rmw_qc_mclicilvl10},
    },
    {
        .csrno = CSR_QC_MCLICILVL11,
        .csr_ops = {"qc_mclicilvl11", pred_qc_mclicilvl11, NULL, NULL, rmw_qc_mclicilvl11},
    },
    {
        .csrno = CSR_QC_MCLICILVL12,
        .csr_ops = {"qc_mclicilvl12", pred_qc_mclicilvl12, NULL, NULL, rmw_qc_mclicilvl12},
    },
    {
        .csrno = CSR_QC_MCLICILVL13,
        .csr_ops = {"qc_mclicilvl13", pred_qc_mclicilvl13, NULL, NULL, rmw_qc_mclicilvl13},
    },
    {
        .csrno = CSR_QC_MCLICILVL14,
        .csr_ops = {"qc_mclicilvl14", pred_qc_mclicilvl14, NULL, NULL, rmw_qc_mclicilvl14},
    },
    {
        .csrno = CSR_QC_MCLICILVL15,
        .csr_ops = {"qc_mclicilvl15", pred_qc_mclicilvl15, NULL, NULL, rmw_qc_mclicilvl15},
    },
    {
        .csrno = CSR_QC_MCLICILVL16,
        .csr_ops = {"qc_mclicilvl16", pred_qc_mclicilvl16, NULL, NULL, rmw_qc_mclicilvl16},
    },
    {
        .csrno = CSR_QC_MCLICILVL17,
        .csr_ops = {"qc_mclicilvl17", pred_qc_mclicilvl17, NULL, NULL, rmw_qc_mclicilvl17},
    },
    {
        .csrno = CSR_QC_MCLICILVL18,
        .csr_ops = {"qc_mclicilvl18", pred_qc_mclicilvl18, NULL, NULL, rmw_qc_mclicilvl18},
    },
    {
        .csrno = CSR_QC_MCLICILVL19,
        .csr_ops = {"qc_mclicilvl19", pred_qc_mclicilvl19, NULL, NULL, rmw_qc_mclicilvl19},
    },
    {
        .csrno = CSR_QC_MCLICILVL20,
        .csr_ops = {"qc_mclicilvl20", pred_qc_mclicilvl20, NULL, NULL, rmw_qc_mclicilvl20},
    },
    {
        .csrno = CSR_QC_MCLICILVL21,
        .csr_ops = {"qc_mclicilvl21", pred_qc_mclicilvl21, NULL, NULL, rmw_qc_mclicilvl21},
    },
    {
        .csrno = CSR_QC_MCLICILVL22,
        .csr_ops = {"qc_mclicilvl22", pred_qc_mclicilvl22, NULL, NULL, rmw_qc_mclicilvl22},
    },
    {
        .csrno = CSR_QC_MCLICILVL23,
        .csr_ops = {"qc_mclicilvl23", pred_qc_mclicilvl23, NULL, NULL, rmw_qc_mclicilvl23},
    },
    {
        .csrno = CSR_QC_MCLICILVL24,
        .csr_ops = {"qc_mclicilvl24", pred_qc_mclicilvl24, NULL, NULL, rmw_qc_mclicilvl24},
    },
    {
        .csrno = CSR_QC_MCLICILVL25,
        .csr_ops = {"qc_mclicilvl25", pred_qc_mclicilvl25, NULL, NULL, rmw_qc_mclicilvl25},
    },
    {
        .csrno = CSR_QC_MCLICILVL26,
        .csr_ops = {"qc_mclicilvl26", pred_qc_mclicilvl26, NULL, NULL, rmw_qc_mclicilvl26},
    },
    {
        .csrno = CSR_QC_MCLICILVL27,
        .csr_ops = {"qc_mclicilvl27", pred_qc_mclicilvl27, NULL, NULL, rmw_qc_mclicilvl27},
    },
    {
        .csrno = CSR_QC_MCLICILVL28,
        .csr_ops = {"qc_mclicilvl28", pred_qc_mclicilvl28, NULL, NULL, rmw_qc_mclicilvl28},
    },
    {
        .csrno = CSR_QC_MCLICILVL29,
        .csr_ops = {"qc_mclicilvl29", pred_qc_mclicilvl29, NULL, NULL, rmw_qc_mclicilvl29},
    },
    {
        .csrno = CSR_QC_MCLICILVL30,
        .csr_ops = {"qc_mclicilvl30", pred_qc_mclicilvl30, NULL, NULL, rmw_qc_mclicilvl30},
    },
    {
        .csrno = CSR_QC_MCLICILVL31,
        .csr_ops = {"qc_mclicilvl31", pred_qc_mclicilvl31, NULL, NULL, rmw_qc_mclicilvl31},
    },
    {
        .csrno = CSR_QC_MCLICIP0,
        .csr_ops = {"qc_mclicip0", pred_qc_mclicip0, NULL, NULL, rmw_qc_mclicip0},
    },
    {
        .csrno = CSR_QC_MCLICIP1,
        .csr_ops = {"qc_mclicip1", pred_qc_mclicip1, NULL, NULL, rmw_qc_mclicip1},
    },
    {
        .csrno = CSR_QC_MCLICIP2,
        .csr_ops = {"qc_mclicip2", pred_qc_mclicip2, NULL, NULL, rmw_qc_mclicip2},
    },
    {
        .csrno = CSR_QC_MCLICIP3,
        .csr_ops = {"qc_mclicip3", pred_qc_mclicip3, NULL, NULL, rmw_qc_mclicip3},
    },
    {
        .csrno = CSR_QC_MCLICIP4,
        .csr_ops = {"qc_mclicip4", pred_qc_mclicip4, NULL, NULL, rmw_qc_mclicip4},
    },
    {
        .csrno = CSR_QC_MCLICIP5,
        .csr_ops = {"qc_mclicip5", pred_qc_mclicip5, NULL, NULL, rmw_qc_mclicip5},
    },
    {
        .csrno = CSR_QC_MCLICIP6,
        .csr_ops = {"qc_mclicip6", pred_qc_mclicip6, NULL, NULL, rmw_qc_mclicip6},
    },
    {
        .csrno = CSR_QC_MCLICIP7,
        .csr_ops = {"qc_mclicip7", pred_qc_mclicip7, NULL, NULL, rmw_qc_mclicip7},
    },
    {
        .csrno = CSR_QC_MMCR,
        .csr_ops = {"qc_mmcr", pred_qc_mmcr, NULL, NULL, rmw_qc_mmcr},
    },
    {
        .csrno = CSR_QC_MNTVEC,
        .csr_ops = {"qc_mntvec", pred_qc_mntvec, NULL, NULL, rmw_qc_mntvec},
    },
    {
        .csrno = CSR_QC_MSTKBOTTOMADDR,
        .csr_ops = {"qc_mstkbottomaddr", pred_qc_mstkbottomaddr, NULL, NULL, rmw_qc_mstkbottomaddr},
    },
    {
        .csrno = CSR_QC_MSTKTOPADDR,
        .csr_ops = {"qc_mstktopaddr", pred_qc_mstktopaddr, NULL, NULL, rmw_qc_mstktopaddr},
    },
    {
        .csrno = CSR_QC_MTHREADPTR,
        .csr_ops = {"qc_mthreadptr", pred_qc_mthreadptr, NULL, NULL, rmw_qc_mthreadptr},
    },
    {
        .csrno = CSR_QC_MWPENDADDR0,
        .csr_ops = {"qc_mwpendaddr0", pred_qc_mwpendaddr0, NULL, NULL, rmw_qc_mwpendaddr0},
    },
    {
        .csrno = CSR_QC_MWPENDADDR1,
        .csr_ops = {"qc_mwpendaddr1", pred_qc_mwpendaddr1, NULL, NULL, rmw_qc_mwpendaddr1},
    },
    {
        .csrno = CSR_QC_MWPENDADDR2,
        .csr_ops = {"qc_mwpendaddr2", pred_qc_mwpendaddr2, NULL, NULL, rmw_qc_mwpendaddr2},
    },
    {
        .csrno = CSR_QC_MWPENDADDR3,
        .csr_ops = {"qc_mwpendaddr3", pred_qc_mwpendaddr3, NULL, NULL, rmw_qc_mwpendaddr3},
    },
    {
        .csrno = CSR_QC_MWPSTARTADDR0,
        .csr_ops = {"qc_mwpstartaddr0", pred_qc_mwpstartaddr0, NULL, NULL, rmw_qc_mwpstartaddr0},
    },
    {
        .csrno = CSR_QC_MWPSTARTADDR1,
        .csr_ops = {"qc_mwpstartaddr1", pred_qc_mwpstartaddr1, NULL, NULL, rmw_qc_mwpstartaddr1},
    },
    {
        .csrno = CSR_QC_MWPSTARTADDR2,
        .csr_ops = {"qc_mwpstartaddr2", pred_qc_mwpstartaddr2, NULL, NULL, rmw_qc_mwpstartaddr2},
    },
    {
        .csrno = CSR_QC_MWPSTARTADDR3,
        .csr_ops = {"qc_mwpstartaddr3", pred_qc_mwpstartaddr3, NULL, NULL, rmw_qc_mwpstartaddr3},
    },

    { },
};
