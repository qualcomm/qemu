/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "qemu/osdep.h"
#include "decode.h"
#include "opcodes.h"
#include "insn.h"
#include "iclass.h"
#include "hlx/hlx.h"
#include "hlx/decode_ext_hlx.h"

/*
 * HLX has strict slot constraints:
 * - Slot 0: Store instructions only
 * - Slot 1: Load instructions only
 * - Slot 2: Shift instructions only
 * - Slot 3: Multiply instructions only
 * - Slots 2/3: Common ALU instructions
 */

typedef struct {
    bool store;  /* Slot 0 */
    bool load;   /* Slot 1 */
    bool shift;  /* Slot 2 */
    bool mpy;    /* Slot 3 */
} hlx_pkt_resources_t;

static bool check_hlx_slot_constraints(Packet *pkt, int slot, int insn_idx,
                                       hlx_pkt_resources_t *resources)
{
    int opcode = pkt->insn[insn_idx].opcode;

    if (slot == 0) {
        /* Slot 0: Only store instructions */
        if (!GET_ATTRIB(opcode, A_STORE)) {
            return false;
        }
        if (resources->store) {
            /* Only one store per packet */
            return false;
        }
        resources->store = true;
    } else if (slot == 1) {
        /* Slot 1: Only load instructions */
        if (!GET_ATTRIB(opcode, A_LOAD)) {
            return false;
        }
        if (resources->load) {
            /* Only one load per packet */
            return false;
        }
        resources->load = true;
    } else if (slot == 2) {
        /* Slot 2: Shift or common ALU instructions */
        if (!GET_ATTRIB(opcode, A_HLX_SHIFT) &&
            !GET_ATTRIB(opcode, A_HLX_COMMON_ALU)) {
            return false;
        }
        if (resources->shift) {
            /* Only one shift per packet */
            return false;
        }
        resources->shift = true;
    } else if (slot == 3) {
        /* Slot 3: Multiply or common ALU instructions */
        if (!GET_ATTRIB(opcode, A_HLX_MPY) &&
            !GET_ATTRIB(opcode, A_HLX_COMMON_ALU)) {
            return false;
        }
        if (resources->mpy) {
            /* Only one multiply per packet */
            return false;
        }
        resources->mpy = true;
    }

    return true;
}

/*
 * Decode checks for HLX instructions in a packet
 *
 * Validates:
 * - Slot assignments match instruction types
 * - Resource conflicts within packet
 *
 * Returns false if the packet is invalid.
 *
 * Note: Register pair constraints (odd:even) are validated during
 * instruction decode/generation, not at packet level.
 */
bool hlx_ext_decode_checks(Packet *pkt, bool disas_only)
{
    hlx_pkt_resources_t resources = {
        .store = false,
        .load = false,
        .shift = false,
        .mpy = false
    };

    /* Slots are only assigned when the packet is being executed */
    if (disas_only) {
        return true;
    }

    for (int i = 0; i < pkt->num_insns; i++) {
        if (GET_ATTRIB(pkt->insn[i].opcode, A_HLX) &&
            !check_hlx_slot_constraints(pkt, pkt->insn[i].slot, i,
                                        &resources)) {
            return false;
        }
    }
    return true;
}

/*
 * Find valid slots for HLX instruction
 * Returns a bitmask of valid slots based on instruction attributes
 */
SlotMask hlx_ext_decode_find_iclass_slots(int opcode)
{
    /* Check specific slot restrictions first */
    if (GET_ATTRIB(opcode, A_RESTRICT_SLOT0ONLY) ||
        GET_ATTRIB(opcode, A_STORE)) {
        return SLOTS_0;
    }
    if (GET_ATTRIB(opcode, A_RESTRICT_SLOT1ONLY) ||
        GET_ATTRIB(opcode, A_LOAD)) {
        return SLOTS_1;
    }
    if (GET_ATTRIB(opcode, A_RESTRICT_SLOT2ONLY) ||
        GET_ATTRIB(opcode, A_HLX_SHIFT)) {
        return SLOTS_2;
    }
    if (GET_ATTRIB(opcode, A_RESTRICT_SLOT3ONLY) ||
        GET_ATTRIB(opcode, A_HLX_MPY)) {
        return SLOTS_3;
    }
    if (GET_ATTRIB(opcode, A_HLX_COMMON_ALU) ||
        GET_ATTRIB(opcode, A_HLX_SLOT23)) {
        return SLOTS_23;  /* Can use slot 2 or 3 */
    }
    if (GET_ATTRIB(opcode, A_HLX_ANY_RESOURCE)) {
        return SLOTS_0123;
    }

    /* Default: shouldn't reach here for HLX instructions */
    g_assert_not_reached();
    return SLOTS_0123;
}
