static bool trans_qc_cm_mva01s(rv_decode *dec, arg_qc_cm_mva01s *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    dec->rs1 = arg->r1s;
    dec->rs2 = arg->r2s;
    dec->op = rv_op_qc_cm_mva01s;
    return true;
}
static bool trans_qc_cm_mvsa01(rv_decode *dec, arg_qc_cm_mvsa01 *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    dec->rs1 = arg->r1s;
    dec->rs2 = arg->r2s;
    dec->op = rv_op_qc_cm_mvsa01;
    return true;
}
static bool trans_qc_cm_pop(rv_decode *dec, arg_qc_cm_pop *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    dec->uimm = arg->rlist;
    dec->imm = arg->spimm;
    dec->op = rv_op_qc_cm_pop;
    return true;
}
static bool trans_qc_cm_popret(rv_decode *dec, arg_qc_cm_popret *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    dec->uimm = arg->rlist;
    dec->imm = arg->spimm;
    dec->op = rv_op_qc_cm_popret;
    return true;
}
static bool trans_qc_cm_popretz(rv_decode *dec, arg_qc_cm_popretz *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    dec->uimm = arg->rlist;
    dec->imm = arg->spimm;
    dec->op = rv_op_qc_cm_popretz;
    return true;
}
static bool trans_qc_cm_push(rv_decode *dec, arg_qc_cm_push *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3) {
        return false;
    }
    arg->spimm <<= 4;
    dec->uimm = arg->rlist;
    dec->imm = arg->spimm;
    dec->op = rv_op_qc_cm_push;
    return true;
}
static bool trans_qc_cm_pushfp(rv_decode *dec, arg_qc_cm_pushfp *arg)
{
    if (!dec->cfg->ext_xqccmp) {
        return false;
    }
    if (arg->rlist == 0 || arg->rlist == 1 || arg->rlist == 2 || arg->rlist == 3 || arg->rlist == 4) {
        return false;
    }
    arg->spimm <<= 4;
    dec->uimm = arg->rlist;
    dec->imm = arg->spimm;
    dec->op = rv_op_qc_cm_pushfp;
    return true;
}
