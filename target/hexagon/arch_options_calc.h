/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ARCH_OPTIONS_CALC_H_
#define _ARCH_OPTIONS_CALC_H_

#ifndef CONFIG_USER_ONLY

#include "hex_mmu.h"

#define VTCM_CFG_BASE_OFF 0x38
#define VTCM_CFG_SIZE_OFF 0x3c

#define in_vtcm_space(ENV, PADDR) \
    in_vtcm_space_impl(ENV, PADDR)

static inline bool in_vtcm_space_impl(thread_t *thread, paddr_t paddr)
{
    static gsize init_needed;
    static paddr_t vtcm_base;
    static target_ulong vtcm_size;

    if (g_once_init_enter(&init_needed)) {
        init_needed = false;
        hwaddr cfgbase = (hwaddr)ARCH_GET_SYSTEM_REG(thread, HEX_SREG_CFGBASE)
            << 16;
        cpu_physical_memory_read(cfgbase + VTCM_CFG_BASE_OFF, &vtcm_base,
            sizeof(target_ulong));
        vtcm_base <<= 16;
        cpu_physical_memory_read(cfgbase + VTCM_CFG_SIZE_OFF, &vtcm_size,
            sizeof(target_ulong));
        vtcm_size *= 1024;
        g_once_init_leave(&init_needed, 1);
    }

    if (paddr >= vtcm_base && paddr < (vtcm_base + vtcm_size))
        return true;
    /*
     * This could be a VA that is mapped to VTCM
     */
    paddr_t phys;
    int prot;
    int size;
    int32_t excp;
    if (hex_tlb_find_match(thread, paddr, MMU_DATA_LOAD, &phys, &prot, &size,
         &excp, cpu_mmu_index(thread, false))) {
        if ((excp != HEX_EVENT_PRECISE) &&
            (phys >= vtcm_base && phys < (vtcm_base + vtcm_size))) {
            return true;
        }
    }
    return false;
}
#else
#define in_vtcm_space(ENV, PADDR) 1
#endif

struct ProcessorState;
enum {
    HIDE_WARNING,
    SHOW_WARNING
};
/**********   Arch Options Calculated Functions   **********/


/* Number of extension register/execution contexts available */
int get_ext_contexts(processor_t *proc);

/**********   End ofArch Options Calculated Functions   **********/

#endif
