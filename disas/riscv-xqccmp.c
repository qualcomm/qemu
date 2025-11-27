#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "disas/riscv.h"
#include "disas/riscv-xqccmp.h"

typedef enum {
    rv_op_illegal = 0,
    rv_op_qc_cm_mva01s = 1,
    rv_op_qc_cm_mvsa01,
    rv_op_qc_cm_pop,
    rv_op_qc_cm_popret,
    rv_op_qc_cm_popretz,
    rv_op_qc_cm_push,
    rv_op_qc_cm_pushfp,
} rv_xqccmp_opcode;

static const rv_opcode_data xqccmp_opcode_data[] = {
    { "qc.illegal", rv_codec_illegal, rv_fmt_none, NULL },
    { "qc.cm.mva01s", rv_codec_skip, "O\t2,1", NULL },
    { "qc.cm.mvsa01", rv_codec_skip, "O\t2,1", NULL },
    { "qc.cm.pop", rv_codec_skip, "O\ti,k", NULL },
    { "qc.cm.popret", rv_codec_skip, "O\ti,k", NULL },
    { "qc.cm.popretz", rv_codec_skip, "O\ti,k", NULL },
    { "qc.cm.push", rv_codec_skip, "O\ti,k", NULL },
    { "qc.cm.pushfp", rv_codec_skip, "O\ti,k", NULL },
};

#include "riscv-xqccmp-16-decode.c.inc"
#include "riscv-xqccmp-trans-disas.c.inc"

const rv_opcode_data *decode_xqccmp(rv_decode *dec, rv_isa isa)
{
    rv_inst inst = dec->inst;

    dec->op = rv_op_illegal;
    switch (dec->inst_length) {
    case 2:
        decode_xqccmp_16_impl(dec, inst);
        break;
    }

    if (dec->op == rv_op_illegal) {
        return NULL;
    }
    return &xqccmp_opcode_data[dec->op];
}
