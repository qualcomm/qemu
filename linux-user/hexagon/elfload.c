/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"
#include "target_elf.h"
#include "target/hexagon/cpu.h"


const char *get_elf_cpu_model(uint32_t eflags)
{
    static char buf[32];
    int err;

    switch (eflags) {
    case 0x04:
        return "v5";
    case 0x05:
        return "v55";
    case 0x60:
        return "v60";
    case 0x61:
        return "v61";
    case 0x62:
        return "v62";
    case 0x65:
        return "v65";
    case 0x66:
        return "v66";
    case 0x67:
    case 0x8067:        /* v67t */
        return "v67";
    case 0x68:
        return "v68";
    case 0x69:
        return "v69";
    case 0x71:
    case 0x8071:        /* v71t */
        return "v71";
    case 0x73:
        return "v73";
    }

    err = snprintf(buf, sizeof(buf), "unknown (0x%x)", eflags);
    return err >= 0 && err < sizeof(buf) ? buf : "unknown";
}

abi_ulong get_elf_hwcap(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    abi_ulong hwcaps = 0;
    uint32_t isa_version = cpu->rev_reg & 0xFF;

    switch (isa_version) {
    case 0x66:
        hwcaps |= HWCAP_HEXAGON_ISA_V66;
        break;
    case 0x67:
        hwcaps |= HWCAP_HEXAGON_ISA_V67;
        break;
    case 0x68:
        hwcaps |= HWCAP_HEXAGON_ISA_V68;
        break;
    case 0x69:
        hwcaps |= HWCAP_HEXAGON_ISA_V69;
        break;
    case 0x71:
        hwcaps |= HWCAP_HEXAGON_ISA_V71;
        break;
    case 0x73:
        hwcaps |= HWCAP_HEXAGON_ISA_V73;
        break;
    case 0x79:
        hwcaps |= HWCAP_HEXAGON_ISA_V79;
        break;
    case 0x81:
        hwcaps |= HWCAP_HEXAGON_ISA_V81;
        break;
    default:
        hwcaps |= HWCAP_HEXAGON_ISA_V73;
        break;
    }

    hwcaps |= HWCAP_HEXAGON_HVX;

    return hwcaps;
}

const char *elf_hwcap_str(uint32_t bit)
{
    static const char *hwcap_str[] = {
        [0]  = "v2",   [1]  = "v3",   [2]  = "v4",   [3]  = "v5",
        [4]  = "v55",  [5]  = "v60",  [6]  = "v62",  [7]  = "v65",
        [8]  = "v66",  [9]  = "v67",  [10] = "v68",  [11] = "v69",
        [12] = "v71",  [13] = "v73",  [14] = "v79",  [15] = "v81",
        [16] = NULL,   [17] = NULL,   [18] = NULL,   [19] = NULL,
        [20] = NULL,   [21] = NULL,   [22] = NULL,   [23] = "hvx",
        [24] = "cabac", [25] = "hvx_length_128b", [26] = "hvx_ieee_fp",
        [27] = "audio", [28] = NULL,   [29] = NULL,   [30] = NULL,
        [31] = NULL,
    };

    return bit < ARRAY_SIZE(hwcap_str) ? hwcap_str[bit] : NULL;
}
