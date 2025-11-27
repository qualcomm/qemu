static bool trans_qc_addsat(rv_decode *dec, arg_qc_addsat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_addsat;
    return true;
}
static bool trans_qc_addusat(rv_decode *dec, arg_qc_addusat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_addusat;
    return true;
}
static bool trans_qc_beqi(rv_decode *dec, arg_qc_beqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_beqi;
    return true;
}
static bool trans_qc_bgei(rv_decode *dec, arg_qc_bgei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_bgei;
    return true;
}
static bool trans_qc_bgeui(rv_decode *dec, arg_qc_bgeui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_bgeui;
    return true;
}
static bool trans_qc_blti(rv_decode *dec, arg_qc_blti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_blti;
    return true;
}
static bool trans_qc_bltui(rv_decode *dec, arg_qc_bltui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_bltui;
    return true;
}
static bool trans_qc_bnei(rv_decode *dec, arg_qc_bnei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_bnei;
    return true;
}
static bool trans_qc_brev32(rv_decode *dec, arg_qc_brev32 *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_brev32;
    return true;
}
static bool trans_qc_c_bexti(rv_decode *dec, arg_qc_c_bexti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->shamt == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_bexti;
    return true;
}
static bool trans_qc_c_bseti(rv_decode *dec, arg_qc_c_bseti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->shamt == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_bseti;
    return true;
}
static bool trans_qc_c_clrint(rv_decode *dec, arg_qc_c_clrint *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_c_clrint;
    return true;
}
static bool trans_qc_c_delay(rv_decode *dec, arg_qc_c_delay *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    if (arg->imm == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_c_delay;
    return true;
}
static bool trans_qc_c_di(rv_decode *dec, arg_qc_c_di *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_di;
    return true;
}
static bool trans_qc_c_dir(rv_decode *dec, arg_qc_c_dir *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_dir;
    return true;
}
static bool trans_qc_c_ei(rv_decode *dec, arg_qc_c_ei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_ei;
    return true;
}
static bool trans_qc_c_eir(rv_decode *dec, arg_qc_c_eir *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_c_eir;
    return true;
}
static bool trans_qc_c_extu(rv_decode *dec, arg_qc_c_extu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->width_minus1 == 0 || arg->width_minus1 == 1 || arg->width_minus1 == 2 || arg->width_minus1 == 3 || arg->width_minus1 == 4) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_extu;
    return true;
}
static bool trans_qc_c_mienter_nest(rv_decode *dec, arg_qc_c_mienter_nest *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_mienter_nest;
    return true;
}
static bool trans_qc_c_mienter(rv_decode *dec, arg_qc_c_mienter *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_mienter;
    return true;
}
static bool trans_qc_c_mileaveret(rv_decode *dec, arg_qc_c_mileaveret *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_mileaveret;
    return true;
}
static bool trans_qc_c_mnret(rv_decode *dec, arg_qc_c_mnret *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_mnret;
    return true;
}
static bool trans_qc_c_mret(rv_decode *dec, arg_qc_c_mret *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->op = rv_op_qc_c_mret;
    return true;
}
static bool trans_qc_c_muliadd(rv_decode *dec, arg_qc_c_muliadd *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciac) {
        return false;
    }
    dec->uimm = arg->uimm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_muliadd;
    return true;
}
static bool trans_qc_c_mveqz(rv_decode *dec, arg_qc_c_mveqz *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_c_mveqz;
    return true;
}
static bool trans_qc_c_ptrace(rv_decode *dec, arg_qc_c_ptrace *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->op = rv_op_qc_c_ptrace;
    return true;
}
static bool trans_qc_c_setint(rv_decode *dec, arg_qc_c_setint *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_c_setint;
    return true;
}
static bool trans_qc_c_sync(rv_decode *dec, arg_qc_c_sync *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->uimm = arg->slist;
    dec->op = rv_op_qc_c_sync;
    return true;
}
static bool trans_qc_c_syncr(rv_decode *dec, arg_qc_c_syncr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->uimm = arg->slist;
    dec->op = rv_op_qc_c_syncr;
    return true;
}
static bool trans_qc_c_syncwf(rv_decode *dec, arg_qc_c_syncwf *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->uimm = arg->slist;
    dec->op = rv_op_qc_c_syncwf;
    return true;
}
static bool trans_qc_c_syncwl(rv_decode *dec, arg_qc_c_syncwl *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->uimm = arg->slist;
    dec->op = rv_op_qc_c_syncwl;
    return true;
}
static bool trans_qc_clo(rv_decode *dec, arg_qc_clo *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_clo;
    return true;
}
static bool trans_qc_clrinti(rv_decode *dec, arg_qc_clrinti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_clrinti;
    return true;
}
static bool trans_qc_compress2(rv_decode *dec, arg_qc_compress2 *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_compress2;
    return true;
}
static bool trans_qc_compress3(rv_decode *dec, arg_qc_compress3 *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_compress3;
    return true;
}
static bool trans_qc_csrrwr(rv_decode *dec, arg_qc_csrrwr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicsr) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_csrrwr;
    return true;
}
static bool trans_qc_csrrwri(rv_decode *dec, arg_qc_csrrwri *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicsr) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_csrrwri;
    return true;
}
static bool trans_qc_cto(rv_decode *dec, arg_qc_cto *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_cto;
    return true;
}
static bool trans_qc_e_addai(rv_decode *dec, arg_qc_e_addai *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_addai;
    return true;
}
static bool trans_qc_e_addi(rv_decode *dec, arg_qc_e_addi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_addi;
    return true;
}
static bool trans_qc_e_andai(rv_decode *dec, arg_qc_e_andai *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_andai;
    return true;
}
static bool trans_qc_e_andi(rv_decode *dec, arg_qc_e_andi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_andi;
    return true;
}
static bool trans_qc_e_beqi(rv_decode *dec, arg_qc_e_beqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_beqi;
    return true;
}
static bool trans_qc_e_bgei(rv_decode *dec, arg_qc_e_bgei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_bgei;
    return true;
}
static bool trans_qc_e_bgeui(rv_decode *dec, arg_qc_e_bgeui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_bgeui;
    return true;
}
static bool trans_qc_e_blti(rv_decode *dec, arg_qc_e_blti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_blti;
    return true;
}
static bool trans_qc_e_bltui(rv_decode *dec, arg_qc_e_bltui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_bltui;
    return true;
}
static bool trans_qc_e_bnei(rv_decode *dec, arg_qc_e_bnei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibi) {
        return false;
    }
    arg->offset <<= 1;
    if (arg->imm == 0) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    dec->offset = arg->offset;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_bnei;
    return true;
}
static bool trans_qc_e_j(rv_decode *dec, arg_qc_e_j *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilb) {
        return false;
    }
    arg->imm <<= 1;
    dec->imm = arg->imm;
    dec->op = rv_op_qc_e_j;
    return true;
}
static bool trans_qc_e_jal(rv_decode *dec, arg_qc_e_jal *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilb) {
        return false;
    }
    arg->imm <<= 1;
    dec->imm = arg->imm;
    dec->op = rv_op_qc_e_jal;
    return true;
}
static bool trans_qc_e_lb(rv_decode *dec, arg_qc_e_lb *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_lb;
    return true;
}
static bool trans_qc_e_lbu(rv_decode *dec, arg_qc_e_lbu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_lbu;
    return true;
}
static bool trans_qc_e_lh(rv_decode *dec, arg_qc_e_lh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_lh;
    return true;
}
static bool trans_qc_e_lhu(rv_decode *dec, arg_qc_e_lhu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_lhu;
    return true;
}
static bool trans_qc_e_li(rv_decode *dec, arg_qc_e_li *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcili) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_li;
    return true;
}
static bool trans_qc_e_lw(rv_decode *dec, arg_qc_e_lw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_lw;
    return true;
}
static bool trans_qc_e_orai(rv_decode *dec, arg_qc_e_orai *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_orai;
    return true;
}
static bool trans_qc_e_ori(rv_decode *dec, arg_qc_e_ori *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_ori;
    return true;
}
static bool trans_qc_e_sb(rv_decode *dec, arg_qc_e_sb *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_sb;
    return true;
}
static bool trans_qc_e_sh(rv_decode *dec, arg_qc_e_sh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_sh;
    return true;
}
static bool trans_qc_e_sw(rv_decode *dec, arg_qc_e_sw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilo) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_e_sw;
    return true;
}
static bool trans_qc_e_xorai(rv_decode *dec, arg_qc_e_xorai *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_xorai;
    return true;
}
static bool trans_qc_e_xori(rv_decode *dec, arg_qc_e_xori *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_e_xori;
    return true;
}
static bool trans_qc_expand2(rv_decode *dec, arg_qc_expand2 *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_expand2;
    return true;
}
static bool trans_qc_expand3(rv_decode *dec, arg_qc_expand3 *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_expand3;
    return true;
}
static bool trans_qc_ext(rv_decode *dec, arg_qc_ext *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_ext;
    return true;
}
static bool trans_qc_extd(rv_decode *dec, arg_qc_extd *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extd;
    return true;
}
static bool trans_qc_extdpr(rv_decode *dec, arg_qc_extdpr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdpr;
    return true;
}
static bool trans_qc_extdprh(rv_decode *dec, arg_qc_extdprh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdprh;
    return true;
}
static bool trans_qc_extdr(rv_decode *dec, arg_qc_extdr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdr;
    return true;
}
static bool trans_qc_extdu(rv_decode *dec, arg_qc_extdu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 31) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdu;
    return true;
}
static bool trans_qc_extdupr(rv_decode *dec, arg_qc_extdupr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdupr;
    return true;
}
static bool trans_qc_extduprh(rv_decode *dec, arg_qc_extduprh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extduprh;
    return true;
}
static bool trans_qc_extdur(rv_decode *dec, arg_qc_extdur *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
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
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extdur;
    return true;
}
static bool trans_qc_extu(rv_decode *dec, arg_qc_extu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_extu;
    return true;
}
static bool trans_qc_insb(rv_decode *dec, arg_qc_insb *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insb;
    return true;
}
static bool trans_qc_insbh(rv_decode *dec, arg_qc_insbh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbh;
    return true;
}
static bool trans_qc_insbhr(rv_decode *dec, arg_qc_insbhr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbhr;
    return true;
}
static bool trans_qc_insbi(rv_decode *dec, arg_qc_insbi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm1 = arg->width_minus1;
    dec->uimm = arg->shamt;
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbi;
    return true;
}
static bool trans_qc_insbpr(rv_decode *dec, arg_qc_insbpr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbpr;
    return true;
}
static bool trans_qc_insbprh(rv_decode *dec, arg_qc_insbprh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbprh;
    return true;
}
static bool trans_qc_insbr(rv_decode *dec, arg_qc_insbr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbr;
    return true;
}
static bool trans_qc_insbri(rv_decode *dec, arg_qc_insbri *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcibm) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_insbri;
    return true;
}
static bool trans_qc_inw(rv_decode *dec, arg_qc_inw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciio) {
        return false;
    }
    arg->imm <<= 2;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_inw;
    return true;
}
static bool trans_qc_li(rv_decode *dec, arg_qc_li *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcili) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_li;
    return true;
}
static bool trans_qc_lieq(rv_decode *dec, arg_qc_lieq *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lieq;
    return true;
}
static bool trans_qc_lieqi(rv_decode *dec, arg_qc_lieqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lieqi;
    return true;
}
static bool trans_qc_lige(rv_decode *dec, arg_qc_lige *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lige;
    return true;
}
static bool trans_qc_ligei(rv_decode *dec, arg_qc_ligei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_ligei;
    return true;
}
static bool trans_qc_ligeu(rv_decode *dec, arg_qc_ligeu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_ligeu;
    return true;
}
static bool trans_qc_ligeui(rv_decode *dec, arg_qc_ligeui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_ligeui;
    return true;
}
static bool trans_qc_lilt(rv_decode *dec, arg_qc_lilt *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lilt;
    return true;
}
static bool trans_qc_lilti(rv_decode *dec, arg_qc_lilti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lilti;
    return true;
}
static bool trans_qc_liltu(rv_decode *dec, arg_qc_liltu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_liltu;
    return true;
}
static bool trans_qc_liltui(rv_decode *dec, arg_qc_liltui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_liltui;
    return true;
}
static bool trans_qc_line(rv_decode *dec, arg_qc_line *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_line;
    return true;
}
static bool trans_qc_linei(rv_decode *dec, arg_qc_linei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicli) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->imm1 = arg->simm;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_linei;
    return true;
}
static bool trans_qc_lrb(rv_decode *dec, arg_qc_lrb *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lrb;
    return true;
}
static bool trans_qc_lrbu(rv_decode *dec, arg_qc_lrbu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lrbu;
    return true;
}
static bool trans_qc_lrh(rv_decode *dec, arg_qc_lrh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lrh;
    return true;
}
static bool trans_qc_lrhu(rv_decode *dec, arg_qc_lrhu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lrhu;
    return true;
}
static bool trans_qc_lrw(rv_decode *dec, arg_qc_lrw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lrw;
    return true;
}
static bool trans_qc_lwm(rv_decode *dec, arg_qc_lwm *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lwm;
    return true;
}
static bool trans_qc_lwmi(rv_decode *dec, arg_qc_lwmi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->imm1 = arg->length;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_lwmi;
    return true;
}
static bool trans_qc_muliadd(rv_decode *dec, arg_qc_muliadd *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciac) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_muliadd;
    return true;
}
static bool trans_qc_mveq(rv_decode *dec, arg_qc_mveq *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mveq;
    return true;
}
static bool trans_qc_mveqi(rv_decode *dec, arg_qc_mveqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mveqi;
    return true;
}
static bool trans_qc_mvge(rv_decode *dec, arg_qc_mvge *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvge;
    return true;
}
static bool trans_qc_mvgei(rv_decode *dec, arg_qc_mvgei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvgei;
    return true;
}
static bool trans_qc_mvgeu(rv_decode *dec, arg_qc_mvgeu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvgeu;
    return true;
}
static bool trans_qc_mvgeui(rv_decode *dec, arg_qc_mvgeui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvgeui;
    return true;
}
static bool trans_qc_mvlt(rv_decode *dec, arg_qc_mvlt *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvlt;
    return true;
}
static bool trans_qc_mvlti(rv_decode *dec, arg_qc_mvlti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvlti;
    return true;
}
static bool trans_qc_mvltu(rv_decode *dec, arg_qc_mvltu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvltu;
    return true;
}
static bool trans_qc_mvltui(rv_decode *dec, arg_qc_mvltui *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvltui;
    return true;
}
static bool trans_qc_mvne(rv_decode *dec, arg_qc_mvne *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvne;
    return true;
}
static bool trans_qc_mvnei(rv_decode *dec, arg_qc_mvnei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcicm) {
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
    dec->rs1 = arg->rs1;
    dec->imm = arg->imm;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_mvnei;
    return true;
}
static bool trans_qc_norm(rv_decode *dec, arg_qc_norm *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_norm;
    return true;
}
static bool trans_qc_normeu(rv_decode *dec, arg_qc_normeu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_normeu;
    return true;
}
static bool trans_qc_normu(rv_decode *dec, arg_qc_normu *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_normu;
    return true;
}
static bool trans_qc_outw(rv_decode *dec, arg_qc_outw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciio) {
        return false;
    }
    arg->imm <<= 2;
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->op = rv_op_qc_outw;
    return true;
}
static bool trans_qc_pcoredump(rv_decode *dec, arg_qc_pcoredump *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->op = rv_op_qc_pcoredump;
    return true;
}
static bool trans_qc_pexit(rv_decode *dec, arg_qc_pexit *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_pexit;
    return true;
}
static bool trans_qc_ppreg(rv_decode *dec, arg_qc_ppreg *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_ppreg;
    return true;
}
static bool trans_qc_ppregs(rv_decode *dec, arg_qc_ppregs *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->op = rv_op_qc_ppregs;
    return true;
}
static bool trans_qc_pputc(rv_decode *dec, arg_qc_pputc *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_pputc;
    return true;
}
static bool trans_qc_pputci(rv_decode *dec, arg_qc_pputci *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_pputci;
    return true;
}
static bool trans_qc_pputs(rv_decode *dec, arg_qc_pputs *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_pputs;
    return true;
}
static bool trans_qc_psyscall(rv_decode *dec, arg_qc_psyscall *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->op = rv_op_qc_psyscall;
    return true;
}
static bool trans_qc_psyscalli(rv_decode *dec, arg_qc_psyscalli *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisim) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_psyscalli;
    return true;
}
static bool trans_qc_selecteqi(rv_decode *dec, arg_qc_selecteqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
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
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selecteqi;
    return true;
}
static bool trans_qc_selectieq(rv_decode *dec, arg_qc_selectieq *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectieq;
    return true;
}
static bool trans_qc_selectieqi(rv_decode *dec, arg_qc_selectieqi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectieqi;
    return true;
}
static bool trans_qc_selectiieq(rv_decode *dec, arg_qc_selectiieq *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->simm1;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectiieq;
    return true;
}
static bool trans_qc_selectiine(rv_decode *dec, arg_qc_selectiine *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->imm = arg->simm1;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectiine;
    return true;
}
static bool trans_qc_selectine(rv_decode *dec, arg_qc_selectine *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectine;
    return true;
}
static bool trans_qc_selectinei(rv_decode *dec, arg_qc_selectinei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->imm1 = arg->simm2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectinei;
    return true;
}
static bool trans_qc_selectnei(rv_decode *dec, arg_qc_selectnei *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcics) {
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
    dec->imm = arg->imm;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_selectnei;
    return true;
}
static bool trans_qc_setinti(rv_decode *dec, arg_qc_setinti *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciint) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_setinti;
    return true;
}
static bool trans_qc_setwm(rv_decode *dec, arg_qc_setwm *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_setwm;
    return true;
}
static bool trans_qc_setwmi(rv_decode *dec, arg_qc_setwmi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->imm1 = arg->length;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_setwmi;
    return true;
}
static bool trans_qc_shladd(rv_decode *dec, arg_qc_shladd *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqciac) {
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
    dec->uimm = arg->shamt;
    dec->rs2 = arg->rs2;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_shladd;
    return true;
}
static bool trans_qc_shlsat(rv_decode *dec, arg_qc_shlsat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_shlsat;
    return true;
}
static bool trans_qc_shlusat(rv_decode *dec, arg_qc_shlusat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_shlusat;
    return true;
}
static bool trans_qc_srb(rv_decode *dec, arg_qc_srb *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_srb;
    return true;
}
static bool trans_qc_srh(rv_decode *dec, arg_qc_srh *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_srh;
    return true;
}
static bool trans_qc_srw(rv_decode *dec, arg_qc_srw *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisls) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    dec->uimm = arg->shamt;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_srw;
    return true;
}
static bool trans_qc_subsat(rv_decode *dec, arg_qc_subsat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_subsat;
    return true;
}
static bool trans_qc_subusat(rv_decode *dec, arg_qc_subusat *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
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
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_subusat;
    return true;
}
static bool trans_qc_swm(rv_decode *dec, arg_qc_swm *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_swm;
    return true;
}
static bool trans_qc_swmi(rv_decode *dec, arg_qc_swmi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcilsm) {
        return false;
    }
    arg->imm <<= 2;
    if (arg->length == 0) {
        return false;
    }
    if (arg->rs3 == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->imm1 = arg->length;
    dec->rs3 = arg->rs3;
    dec->op = rv_op_qc_swmi;
    return true;
}
static bool trans_qc_sync(rv_decode *dec, arg_qc_sync *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_sync;
    return true;
}
static bool trans_qc_syncr(rv_decode *dec, arg_qc_syncr *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_syncr;
    return true;
}
static bool trans_qc_syncwf(rv_decode *dec, arg_qc_syncwf *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_syncwf;
    return true;
}
static bool trans_qc_syncwl(rv_decode *dec, arg_qc_syncwl *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcisync) {
        return false;
    }
    dec->imm = arg->imm;
    dec->op = rv_op_qc_syncwl;
    return true;
}
static bool trans_qc_wrap(rv_decode *dec, arg_qc_wrap *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
        return false;
    }
    if (arg->rs2 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->rs1 = arg->rs1;
    dec->rs2 = arg->rs2;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_wrap;
    return true;
}
static bool trans_qc_wrapi(rv_decode *dec, arg_qc_wrapi *arg)
{
    if (!dec->cfg->ext_xqci && !dec->cfg->ext_xqcia) {
        return false;
    }
    if (arg->rs1 == 0) {
        return false;
    }
    if (arg->rd == 0) {
        return false;
    }
    dec->imm = arg->imm;
    dec->rs1 = arg->rs1;
    dec->rd = arg->rd;
    dec->op = rv_op_qc_wrapi;
    return true;
}
