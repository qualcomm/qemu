/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"
#include "target_elf.h"
#include "target/hexagon/cpu.h"


abi_ulong get_elf_hwcap(CPUState *cs)
{
    HexagonCPUClass *hcc = HEXAGON_CPU_GET_CLASS(cs);
    abi_ulong hwcaps = 0;
    uint32_t hex_ver;

    if (!hcc->hex_def) {
        return 0;
    }

    hex_ver = hcc->hex_def->hex_version;

    switch (hex_ver) {
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
    case 0x75:
        hwcaps |= HWCAP_HEXAGON_ISA_V75;
        break;
    case 0x77:
        hwcaps |= HWCAP_HEXAGON_ISA_V77;
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

    hwcaps |= HWCAP_HEXAGON_HVX | HWCAP_HEXAGON_HVX_LENGTH_128B;
    if (hex_ver >= 0x68) {
        hwcaps |= HWCAP_HEXAGON_HVX_IEEE_FP;
    }

    return hwcaps;
}

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
    case 0x75:   /* v75 */
        return "v75";
    case 0x77:   /* v77 */
        return "v77";
    case 0x79:   /* v79 */
        return "v79";
    case 0x81:
        return "v81";
    }

    err = snprintf(buf, sizeof(buf), "unknown (0x%x)", eflags);
    return err >= 0 && err < sizeof(buf) ? buf : "unknown";
}
